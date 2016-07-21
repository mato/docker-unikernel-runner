/*
 * Copyright (c) 2016 Martin Lucina <martin.lucina@docker.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>

#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>
#include <netlink/route/link/bridge.h>
#include <netlink/route/route.h>
#include <netlink/route/nexthop.h>

#include <cap-ng.h>

#include "ptrvec.h"

/* Container-side network interface to use */
#define VETH_LINK_NAME   "eth0"
/* Name of bridge interface to create */
#define BRIDGE_LINK_NAME "br0"
/* Name of tap interface to create */
#define TAP_LINK_NAME    "tap0"
/* Buffer size large enough to hold IPv4 adress with CIDR prefix */
#define AF_INET_BUFSIZE  19

/*
 * Create a persistent tap interface. Returns 0 if successful, system errno if
 * not.
 */
static int create_tap_link(const char *name)
{
    struct ifreq ifr;
    int fd;

    if (strlen(name) > IFNAMSIZ)
        return ENAMETOOLONG;

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0)
        return errno;

    ifr.ifr_flags = IFF_TAP;
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    if (ioctl(fd, TUNSETIFF, &ifr) < 0)
        return errno;

    if (ioctl(fd, TUNSETPERSIST, 1) < 0)
        return errno;

    close(fd);

    return 0;
}

/*
 * Create a bridge interface. Returns 0 if successful, libnl error if not.
 */
static int create_bridge_link(struct nl_sock *sk, const char *name)
{
    struct rtnl_link *l_bridge;
    int err;

    l_bridge = rtnl_link_bridge_alloc();
    assert(l_bridge);
    rtnl_link_set_name(l_bridge, name);
    
    err = rtnl_link_add(sk, l_bridge, NLM_F_CREATE);
    if (err < 0) {
        rtnl_link_put(l_bridge);
        return err;
    }
    
    rtnl_link_put(l_bridge);
    return 0;
}

static void match_first_addr(struct nl_object *obj, void *arg)
{
    static int found = 0;
    struct nl_addr **addr = (struct nl_addr **)arg;

    if (found)
        return;

    *addr = rtnl_addr_get_local((struct rtnl_addr *)obj);
    nl_addr_get(*addr); /* Found, keep reference */
    found = 1;
}

/*
 * Get the first AF_INET address on 'link'. Returns 0 if successful. Caller
 * must release reference to *addr.
 */
static int get_link_inet_addr(struct nl_sock *sk, struct rtnl_link *link,
        struct nl_addr **addr)
{
    struct nl_cache *addr_cache;
    int err;
    err = rtnl_addr_alloc_cache(sk, &addr_cache);
    if (err < 0) {
        nl_perror(err, "rtnl_addr_alloc_cache()");
        return 1;
    }

    /* Retrieve the first AF_INET address on the requested interface. */
    struct rtnl_addr *filter;
    filter = rtnl_addr_alloc();
    assert(filter);
    rtnl_addr_set_ifindex(filter, rtnl_link_get_ifindex(link));
    rtnl_addr_set_family(filter, AF_INET);

    *addr = NULL;
    nl_cache_foreach_filter(addr_cache, (struct nl_object *)filter,
            match_first_addr, addr);
    if (*addr == NULL) {
        fprintf(stderr, "No AF_INET address found on veth\n");
        
        rtnl_addr_put(filter);
        nl_cache_free(addr_cache);
        return 1;
    }

    rtnl_addr_put(filter);
    nl_cache_free(addr_cache);
    return 0;
}

static void match_first_nh_gw(struct nl_object *obj, void *arg)
{
    static int found = 0;
    struct rtnl_route *route = (struct rtnl_route *)obj;
    struct nl_addr **gw = (struct nl_addr **)arg;

    if (found)
        return;

    struct rtnl_nexthop *nh = rtnl_route_nexthop_n(route, 0);
    if (nh == NULL)
        return;
    *gw = rtnl_route_nh_get_gateway(nh);
    nl_addr_get(*gw); /* Found, keep reference */
    found = 1;
}

/*
 * Get the nexthop for the first default AF_INET route. Sets
 * *addr if found, caller must release reference to *addr. Returns 1 if an
 * error occurs, NULL *addr if no error but no AF_INET default route exists.
 */
static int get_default_gw_inet_addr(struct nl_sock *sk, struct nl_addr **addr)
{
    struct nl_cache *route_cache;
    int err;
    err = rtnl_route_alloc_cache(sk, AF_INET, 0, &route_cache);
    if (err < 0) {
        nl_perror(err, "rtnl_addr_alloc_cache()");
        return 1;
    }

    /* Retrieve the first AF_INET default route. */
    struct rtnl_route *filter;
    filter = rtnl_route_alloc();
    assert(filter);
    rtnl_route_set_type(filter, 1); /* XXX RTN_UNICAST from linux/rtnetlink.h */
    struct nl_addr *filter_addr;
    err = nl_addr_parse("default", AF_INET, &filter_addr);
    if (err < 0) {
        nl_perror(err, "nl_addr_parse(default)");

        rtnl_route_put(filter);
        nl_cache_free(route_cache);
        return 1;
    }
    rtnl_route_set_dst(filter, filter_addr);

    *addr = NULL;
    nl_cache_foreach_filter(route_cache, (struct nl_object *)filter,
            match_first_nh_gw, addr);

    /* No default gateway is not an error, so always return 0 here */
    nl_addr_put(filter_addr);
    rtnl_route_put(filter);
    nl_cache_free(route_cache);
    return 0;
}

int main(int argc, char *argv[])
{
    char *unikernel;
    enum {
        QEMU,
        KVM,
        UKVM,
        UNIX
    } hypervisor;

    if (argc < 3) {
        fprintf(stderr, "usage: runner HYPERVISOR UNIKERNEL [ ARGS... ]\n");
        fprintf(stderr, "HYPERVISOR: qemu | kvm | ukvm | unix\n");
        return 1;
    }
    if (strcmp(argv[1], "qemu") == 0)
        hypervisor = QEMU;
    else if (strcmp(argv[1], "kvm") == 0)
        hypervisor = KVM;
    else if (strcmp(argv[1], "ukvm") == 0)
        hypervisor = UKVM;
    else if (strcmp(argv[1], "unix") == 0)
        hypervisor = UNIX;
    else {
        fprintf(stderr, "error: Invalid hypervisor: %s\n", argv[1]);
        return 1;
    }
    unikernel = argv[2];
    /*
     * Remaining arguments are to be passed on to the unikernel.
     */
    argv += 3;
    argc -= 3;

    /*
     * Check we have CAP_NET_ADMIN.
     */
    if (capng_get_caps_process() != 0) {
        fprintf(stderr, "error: capng_get_caps_process() failed\n");
        return 1;
    }
    if (!capng_have_capability(CAPNG_EFFECTIVE, CAP_NET_ADMIN)) {
        fprintf(stderr, "error: CAP_NET_ADMIN is required\n");
        return 1;
    }

    /*
     * Connect to netlink, load link cache from kernel.
     */
    struct nl_sock *sk;
    struct nl_cache *link_cache;
    int err;
 
    sk = nl_socket_alloc();
    assert(sk);
    err = nl_connect(sk, NETLINK_ROUTE);
    if (err < 0) {
        nl_perror(err, "nl_connect()");
        return 1;
    }
    err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache);
    if (err < 0) {
        nl_perror(err, "rtnl_link_alloc_cache()");
        return 1;
    }
   
    /*
     * Retrieve container network configuration -- IP address and
     * default gateway.
     */
    struct rtnl_link *l_veth;
    l_veth = rtnl_link_get_by_name(link_cache, VETH_LINK_NAME);
    if (l_veth == NULL) {
        nl_perror(err, "rtnl_link_get_by_name(" VETH_LINK_NAME ")");
        return 1;
    }
    struct nl_addr *veth_addr;
    err = get_link_inet_addr(sk, l_veth, &veth_addr);
    if (err) {
        fprintf(stderr, "error: Unable to determine IP address of %s\n",
                VETH_LINK_NAME);
        return 1;
    }
    struct nl_addr *gw_addr;
    err = get_default_gw_inet_addr(sk, &gw_addr);
    if (err) {
        fprintf(stderr, "error: get_deGfault_gw_inet_addr() failed\n");
        return 1;
    }
    if (gw_addr == NULL) {
        fprintf(stderr, "error: No default gateway found. This is currently "
                "not supported");
        return 1;
    }

    /*
     * Create bridge and tap interface, enslave veth and tap interfaces to
     * bridge.
     */
    err = create_bridge_link(sk, BRIDGE_LINK_NAME);
    if (err < 0) {
        nl_perror(err, "create_bridge_link(" BRIDGE_LINK_NAME ")");
        return 1;
    }
    err = create_tap_link(TAP_LINK_NAME);
    if (err != 0) {
        fprintf(stderr, "create_tap_link(%s): %s\n", TAP_LINK_NAME,
                strerror(errno));
        return 1;
    }

    /* Refill link cache with newly-created interfaces */
    nl_cache_refill(sk, link_cache);

    struct rtnl_link *l_bridge;
    l_bridge = rtnl_link_get_by_name(link_cache, BRIDGE_LINK_NAME);
    if (l_bridge == NULL) {
        nl_perror(err, "rtnl_link_get_by_name(" BRIDGE_LINK_NAME ")");
        return 1;
    }
    struct rtnl_link *l_tap;
    l_tap = rtnl_link_get_by_name(link_cache, TAP_LINK_NAME);
    if (l_tap == NULL) {
        nl_perror(err, "rtnl_link_get_by_name(" TAP_LINK_NAME ")");
        return 1;
    }
    err = rtnl_link_enslave(sk, l_bridge, l_veth);
    if (err < 0) {
        nl_perror(err, "Unable to enslave veth to bridge");
        return 1;
    }
    err = rtnl_link_enslave(sk, l_bridge, l_tap);
    if (err < 0) {
        nl_perror(err, "Unable to enslave tap to bridge");
        return 1;
    }

    /*
     * Flush all IPv4 addresses from the veth interface. This is now safe
     * as we are good to commit and have retrieved the existing configuration.
     */
    struct rtnl_addr *flush_addr;
    flush_addr = rtnl_addr_alloc();
    assert(flush_addr);
    rtnl_addr_set_ifindex(flush_addr, rtnl_link_get_ifindex(l_veth));
    rtnl_addr_set_family(flush_addr, AF_INET);
    rtnl_addr_set_local(flush_addr, veth_addr);
    err = rtnl_addr_delete(sk, flush_addr, 0);
    if (err < 0) {
        nl_perror(err, "rtnl_addr_delete(" VETH_LINK_NAME ")");
        return 1;
    }
    rtnl_addr_put(flush_addr);

    /* 
     * Bring up the tap and bridge interfaces.
     */
    struct rtnl_link *l_up;
    l_up = rtnl_link_alloc();
    assert(l_up);
    /* You'd think set_operstate was the thing to do here. It's not. */
    rtnl_link_set_flags(l_up, IFF_UP);
    err = rtnl_link_change(sk, l_tap, l_up, 0);
    if (err < 0) {
        nl_perror(err, "rtnl_link_change(" TAP_LINK_NAME ", UP)");
        return 1;
    }
    err = rtnl_link_change(sk, l_bridge, l_up, 0);
    if (err < 0) {
        nl_perror(err, "rtnl_link_change(" BRIDGE_LINK_NAME ", UP)");
        return 1;
    }
    rtnl_link_put(l_up);

    /*
     * Collect network configuration data.
     */
    char uarg_ip[AF_INET_BUFSIZE];
    if (inet_ntop(AF_INET, nl_addr_get_binary_addr(veth_addr), uarg_ip,
            sizeof uarg_ip) == NULL) {
        perror("inet_ntop()");
        return 1;
    }

    char uarg_netmask[AF_INET_BUFSIZE];
    in_addr_t netmask = 0;
    unsigned int prefixlen = nl_addr_get_prefixlen(veth_addr);
    for (unsigned int b = 0; b < prefixlen; b++) {
        netmask |= (1 << b);
    }
    if (inet_ntop(AF_INET, &netmask, uarg_netmask,
            sizeof uarg_netmask) == NULL) {
        perror("inet_ntop()");
        return 1;
    }

    char uarg_gw[AF_INET_BUFSIZE];
    if (inet_ntop(AF_INET, nl_addr_get_binary_addr(gw_addr), uarg_gw,
            sizeof uarg_gw) == NULL) {
        perror("inet_ntop()");
        return 1;
    }

    /*
     * Build unikernel and hypervisor arguments.
     */
    ptrvec* uargpv = pvnew();
    char *uarg_buf;
    /*
     * QEMU/KVM:
     * /usr/bin/qemu-system-x86_64 <qemu args> -kernel <unikernel> -append "<unikernel args>"
     */
    if (hypervisor == QEMU || hypervisor == KVM) {
        pvadd(uargpv, "/usr/bin/qemu-system-x86_64");
        pvadd(uargpv, "-vga");
        pvadd(uargpv, "none");
        pvadd(uargpv, "-nographic");
        if (hypervisor == KVM) {
            pvadd(uargpv, "-enable-kvm");
            pvadd(uargpv, "-cpu");
            pvadd(uargpv, "host");
        }
        else {
            /*
             * Required for AESNI use in Mirage.
             */
            pvadd(uargpv, "-cpu");
            pvadd(uargpv, "Broadwell");
        }
        pvadd(uargpv, "-device");
        pvadd(uargpv, "virtio-net,netdev=n0");
        pvadd(uargpv, "-netdev");
        err = asprintf(&uarg_buf, "tap,id=n0,ifname=%s,script=no,downscript=no",
            TAP_LINK_NAME);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        pvadd(uargpv, "-kernel");
        pvadd(uargpv, unikernel);
        pvadd(uargpv, "-append");
        /*
         * TODO: Replace any occurences of ',' with ',,' in -append, because
         * QEMU arguments are insane.
         */
        char cmdline[1024];
        char *cmdline_p = cmdline;
        size_t cmdline_free = sizeof cmdline;
        for (; *argv; argc--, argv++) {
            size_t alen = snprintf(cmdline_p, cmdline_free, "%s%s", *argv,
                    (argc > 1) ? " " : "");
            if (alen >= cmdline_free) {
                fprintf(stderr, "error: Command line too long\n");
                return 1;
            }
            cmdline_free -= alen;
            cmdline_p += alen;
        }
        size_t alen = snprintf(cmdline_p, cmdline_free,
                "--ip=%s --netmask=%s --gateways=%s",
                uarg_ip, uarg_netmask, uarg_gw);
        if (alen >= cmdline_free) {
            fprintf(stderr, "error: Command line too long\n");
            return 1;
        }
        pvadd(uargpv, cmdline);
    }
    /*
     * UKVM:
     * /unikernel/ukvm <ukvm args> <unikernel> -- <unikernel args>
     */
    else if (hypervisor == UKVM) {
        pvadd(uargpv, "/unikernel/ukvm");
        err = asprintf(&uarg_buf, "--net=%s", TAP_LINK_NAME);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        pvadd(uargpv, unikernel);
        pvadd(uargpv, "--");
        for (; *argv; argc--, argv++) {
            pvadd(uargpv, *argv);
        }
        err = asprintf(&uarg_buf, "--ip=%s", uarg_ip);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        err = asprintf(&uarg_buf, "--netmask=%s", uarg_netmask);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        err = asprintf(&uarg_buf, "--gateways=%s", uarg_gw);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
    }
    /*
     * UNIX:
     * <unikernel> <unikernel args>
     */
    else if (hypervisor == UNIX) {
        pvadd(uargpv, unikernel);
        err = asprintf(&uarg_buf, "--network=%s", TAP_LINK_NAME);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        for (; *argv; argc--, argv++) {
            pvadd(uargpv, *argv);
        }
        err = asprintf(&uarg_buf, "--ip=%s", uarg_ip);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        err = asprintf(&uarg_buf, "--netmask=%s", uarg_netmask);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
        err = asprintf(&uarg_buf, "--gateways=%s", uarg_gw);
        assert(err != -1);
        pvadd(uargpv, uarg_buf);
    }
    char **uargv = (char **)pvfinal(uargpv);

    /*
     * Done with netlink, free all resources and close socket.
     */
    rtnl_link_put(l_veth);
    rtnl_link_put(l_bridge);
    rtnl_link_put(l_tap);
    nl_addr_put(veth_addr);
    nl_addr_put(gw_addr);

    nl_cache_free(link_cache);
    nl_close(sk);
    nl_socket_free(sk);

    /*
     * Drop all capabilities except CAP_NET_BIND_SERVICE.
     */
    capng_clear(CAPNG_SELECT_BOTH);
    capng_update(CAPNG_ADD,
            CAPNG_EFFECTIVE | CAPNG_PERMITTED | CAPNG_INHERITABLE,
            CAP_NET_BIND_SERVICE);
    if (capng_apply(CAPNG_SELECT_BOTH) != 0) {
        fprintf(stderr, "error: Could not drop capabilities");
        return 1;
    }

    /*
     * Run the unikernel.
     */
    err = execv(uargv[0], uargv);
    fprintf(stderr, "error: execv() of %s failed: %s", uargv[0],
            strerror(errno));
    return 1;
}

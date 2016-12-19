#!/bin/sh

usage()
{
    ME=$(basename $0)
    cat <<EOM 1>&2
usage: ${ME} COMMAND ARGS... 

Simple wrapper around 'docker build' and 'docker run' to build and run
Mirage unikernels using docker-unikernel-runner.

Available COMMANDs:

run [ OPTIONS ] IMAGE -- Wrapper for 'docker run':
    Adds CAP_NET_ADMIN, passes through host /dev/net/tun and /dev/kvm (if
    available).

build HYPERVISOR [ OPTIONS ] -- Wrapper for 'docker build':
    HYPERVISOR: one of qemu | kvm | ukvm | unix.
    OPTIONS: passed through to 'docker build'.
EOM
    exit 1
}

do_build()
{
    if [ ! -f ./config.ml ]; then
        echo error: No configuration file config.ml found. 1>&2
        echo error: This does not look like a Mirage unikernel source directory. 1>&2
        exit 1
    fi
    [ $# -lt 1 ] && usage
    HYPERVISOR=$1
    case ${HYPERVISOR} in
        ukvm)
            SUFFIX=.ukvm
            BASE=mir-runner
            ADD="ADD ./ukvm-bin /unikernel/ukvm"
            ;;
        kvm|qemu)
            SUFFIX=.virtio
            BASE=mir-runner-qemu
            ;;
        unix)
            SUFFIX=
            BASE=mir-runner
            ;;
        *)
            usage
            ;;
    esac
    shift

    BIN=$(mirage describe | awk -- '/^Name/{print $2}')${SUFFIX}
    if [ ! -f ${BIN} ]; then
        echo error: Unikernel binary \"./${BIN}\" not found. 1>&2
        exit 1
    fi
    if [ "${HYPERVISOR}" = "ukvm" ]; then
        if ! ldd ./ukvm-bin | grep -q "not a dynamic executable"; then
            echo error: ./ukvm-bin must be statically linked. 1>&2
            echo error: Rebuild with \"make UKVM_STATIC=1\" and re-run this command. 1>&2
            exit 1
        fi
    fi

    DOCKERFILE=$(mktemp Dockerfile.XXXXXXXXXX)
    cat <<EOM >${DOCKERFILE}
    FROM ${BASE}
    ${ADD}
    ADD ./${BIN} /unikernel/${BIN}
    ENTRYPOINT [ "/runtime/runner", "${HYPERVISOR}", "/unikernel/${BIN}" ]
EOM
    BUILDIT="docker build -f ${DOCKERFILE} $@ ."
    ${BUILDIT}
    if [ $? -ne 0 ]; then
        echo error: \"${BUILDIT}\" failed. 1>&2
        echo error: Generated Dockerfile left in ${DOCKERFILE}. 1>&2
        exit 1
    fi
    rm -f ${DOCKERFILE}
}

check_arp()
{
    ARP_ACCEPT=$(sysctl -n net.ipv4.conf.docker0.arp_accept)
    [ "${ARP_ACCEPT}" = "1" ] && return
    cat <<EOM 1>&2
>>> WARNING: Run "sysctl -w net.ipv4.conf.docker0.arp_accept=1" to enable
>>> WARNING: gratuitous ARP on the Docker bridge, otherwise you will experience
>>> WARNING: intermittent network failures.
>>> WARNING: Now sleeping 10 seconds before continuing.
EOM
    sleep 10
}

do_run()
{
    check_arp
    if [ -c /dev/kvm -a -w /dev/kvm ]; then
        DEV_KVM="--device /dev/kvm:/dev/kvm"
    else
        DEV_KVM=
    fi
    exec docker run --cap-add NET_ADMIN \
        --device /dev/net/tun:/dev/net/tun \
        ${DEV_KVM} \
        "$@"
}

if [ "$#" -lt 2 ]; then
    usage
fi

case $1 in
    run)
        shift
        do_run "$@"
        ;;
    build)
        shift
        do_build "$@"
        ;;
    *)
        usage
        ;;
esac

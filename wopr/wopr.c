/*
 * Copyright (c) 2013 Antti Kantee.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXCONN 64

struct conn {
	int c_bpos;
	int c_cnt;
	char c_buf[0xe0];
};

static struct pollfd pfds[MAXCONN];
static struct conn conns[MAXCONN];
int maxfd, masterfd;

static void
acceptconn(void)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	int s;

	if ((s = accept(masterfd, (struct sockaddr *)&sin, &slen)) == -1)
		return;

	/* drop */
	if (s >= MAXCONN) {
		close(s);
		return;
	}

	/* init */
	pfds[s].fd = s;
	memset(&conns[s], 0, sizeof(conns[s]));

	/* XXX: not g/c'd */
	if (s+1 > maxfd)
		maxfd = s+1;

#define PROMPT "LOGON: "
	/* just assume this will go into the socket without blocking */
	write(s, PROMPT, sizeof(PROMPT)-1);
#undef PROMPT
}

static void
readconn(int i)
{
	struct conn *c = &conns[i];
	char *p;
	ssize_t nn;

	nn = read(i, c->c_buf+c->c_bpos, sizeof(c->c_buf)-c->c_bpos);
	/* treat errors and EOF the same way, we shouldn't get EAGAIN */
	if (nn <= 0) {
		close(i);
		pfds[i].fd = -1;
		c->c_cnt = -1;
		return;
	}

	if ((p = strchr(c->c_buf, '\n')) == NULL) {
		c->c_bpos += nn;
		return;
	}

	*p = '\0';
#define GREET "GREETINGS PROFESSOR FALKEN.\n"
#define NOPE "LOGIN INCORRECT\n"
	/* multiple holes here, some more microsofty than others */
	if (strncmp(c->c_buf, "Joshua", 6) == 0) {
		write(i, GREET, sizeof(GREET)-1);
	} else if (strncmp(c->c_buf, "reboot", 6) == 0) {
		reboot(0, 0);
	} else if (strncmp(c->c_buf, "shutdown", 6) == 0) {
		exit(0);
	} else {
		write(i, NOPE, sizeof(NOPE)-1);
	}
	pfds[i].fd = -1;
#undef GREET
#undef NOPE
}

static void
processzombies(void)
{
	int i;

	/*
	 * Let each connection live ~10s regardless of whether it's
	 * completed or not.
	 */
	for (i = 1; i < MAXCONN; i++) {
		if (conns[i].c_cnt != -1 && ++conns[i].c_cnt > 10
                    && pfds[i].fd != -1) {
			close(i);
		}
	}
}

static int
sucketonport(uint16_t port)
{
	struct sockaddr_in6 sin;
	int s, off = 0;

	s = socket(PF_INET6, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "socket");
        if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off) == -1)
		err(1, "setsockopt");

	memset(&sin, 0, sizeof(sin));
	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(port);
	sin.sin6_addr = in6addr_any;
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(1, "bind");

	if (listen(s, 10) == -1)
		err(1, "unix man, please listen");

	return s;
}

static void
donet(void)
{
	uint64_t zombietime;
	int rv, i;

	masterfd = sucketonport(4096);
	assert(masterfd < MAXCONN);

	for (i = 0; i < MAXCONN; i++) {
		pfds[i].fd = -1;
		pfds[i].events = POLLIN;
		conns[i].c_cnt = -1;
	}
	pfds[masterfd].fd = masterfd;
	maxfd = masterfd+1;

	printf("WOPR reporting for duty on port 4096\n");

	zombietime = time(NULL);
	for (;;) {
		if (time(NULL) - zombietime >= 1) {
			processzombies();
			zombietime = time(NULL);
		}

		rv = poll(pfds, maxfd, 1000);
		if (rv == 0) {
			printf("still waiting ... %"PRId64"\n", time(NULL));
			continue;
		}

		if (rv == -1) {
			printf("fail poll %d\n", errno);
			reboot(0, 0);
		}

		if (pfds[masterfd].revents & POLLIN) {
			acceptconn();
			rv--;
		}

		for (i = 0; i < MAXCONN && rv; i++) {
			if (i == masterfd)
				continue;
			if (pfds[i].fd != -1 && pfds[i].revents & POLLIN) {
				readconn(i);
				rv--;
			}
		}
		assert(rv == 0);
	}
}

int
main(int argc, char *argv[])
{
	donet();

	return 0;
}

/*** ud-sock.h -- socket helpers
 *
 * Copyright (C) 2009-2014 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
#if !defined INCLUDED_ud_sock_h_
#define INCLUDED_ud_sock_h_
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

typedef union {
	struct sockaddr_storage ss;
	struct sockaddr sa;
	struct sockaddr_in6 s6;
} ud_sockaddr_t;


static inline int
getsockopt_int(int s, int level, int optname)
{
	int res[1];
	socklen_t rsz = sizeof(*res);
	if (getsockopt(s, level, optname, res, &rsz) >= 0) {
		return *res;
	}
	return -1;
}

static inline int
setsockopt_int(int s, int level, int optname, int value)
{
	return setsockopt(s, level, optname, &value, sizeof(value));
}

static inline int
setsockopt_tv(int s, int level, int optname, unsigned int value)
{
	struct timeval tv = {
		.tv_sec = value / 1000000U,
		.tv_usec = value % 1000000U,
	};
	return setsockopt(s, level, optname, &tv, sizeof(tv));
}


/* some prominent (boolean) options */
/**
 * Set linger time. */
static inline int
setsock_linger(int s, int ltime)
{
#if defined SO_LINGER
	struct linger lng[1] = {{.l_onoff = 1, .l_linger = ltime}};
	return setsockopt(s, SOL_SOCKET, SO_LINGER, lng, sizeof(*lng));
#else  /* !SO_LINGER */
	return 0;
#endif	/* SO_LINGER */
}

/**
 * Mark address behind socket S as reusable. */
static inline int
setsock_reuseaddr(int s)
{
#if defined SO_REUSEADDR
	return setsockopt_int(s, SOL_SOCKET, SO_REUSEADDR, 1);
#else  /* !SO_REUSEADDR */
	return 0;
#endif	/* SO_REUSEADDR */
}

/* probably only available on BSD */
static inline int
setsock_reuseport(int __attribute__((unused)) s)
{
#if defined SO_REUSEPORT
	return setsockopt_int(s, SOL_SOCKET, SO_REUSEPORT, 1);
#else  /* !SO_REUSEPORT */
	return 0;
#endif	/* SO_REUSEPORT */
}

/**
 * Impose a receive timeout upon S, TIMEO in microseconds. */
static inline int
setsock_rcvtimeo(int s, unsigned int timeo)
{
	return setsockopt_tv(s, SOL_SOCKET, SO_RCVTIMEO, timeo);
}

/**
 * Impose a send timeout upon S, TIMEO in microseconds. */
static inline int
setsock_sndtimeo(int s, unsigned int timeo)
{
	return setsockopt_tv(s, SOL_SOCKET, SO_SNDTIMEO, timeo);
}

/**
 * Do not delay packets, send them right off. */
static inline int
setsock_nodelay(int s)
{
#if defined TCP_NODELAY && defined SOL_TCP
	return setsockopt_int(s, SOL_TCP, TCP_NODELAY, 1);
#else  /* !TCP_NODELAY */
	return 0;
#endif	/* TCP_NODELAY */
}

static __attribute__((unused)) void
setsock_nonblock(int sock)
{
	int opts;

	/* get former options */
	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		return;
	}
	opts |= O_NONBLOCK;
	(void)fcntl(sock, F_SETFL, opts);
	return;
}

static inline int
tcp_cork(int s)
{
#if defined TCP_CORK && defined SOL_TCP
	return setsockopt_int(s, SOL_TCP, TCP_CORK, 1);
#else  /* !TCP_CORK */
	/* be tough, if someone wants a cork and it's not
	 * supported we're fucked, so return failure here */
	return -1;
#endif	/* TCP_CORK */
}

static inline int
tcp_uncork(int s)
{
#if defined TCP_CORK && defined SOL_TCP
	return setsockopt_int(s, SOL_TCP, TCP_CORK, 0);
#else  /* !TCP_CORK */
	/* be tough, if someone wants a cork and it's not
	 * supported we're fucked, so return failure here */
	return -1;
#endif	/* TCP_CORK */
}


/* stuff operating on our ud_sockaddr_t */
static inline __attribute__((const, pure)) short unsigned int
ud_sockaddr_fam(ud_sockaddr_t sa)
{
	return sa.sa.sa_family;
}

static inline __attribute__((const, pure)) short unsigned int
ud_sockaddr_port(ud_sockaddr_t sa)
{
	return ntohs(sa.s6.sin6_port);
}

static inline int
ud_sockaddr_ntop(char *restrict buf, size_t len, ud_sockaddr_t sa)
{
	short unsigned int fam = ud_sockaddr_fam(sa);
	if (inet_ntop(fam, (void*)&sa.s6.sin6_addr, buf, len) == NULL) {
		return -1;
	}
	return 0;
}

#endif	/* INCLUDED_ud_sock_h_ */

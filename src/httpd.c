/*** httpd.c -- our own take on serving via http
 *
 * Copyright (C) 2010-2014 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of gandalf.
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <ev.h>
#include "ud-sock.h"
#include "httpd.h"
#include "logger.h"
#include "nifty.h"

/* private version of struct gand_httpd_s */
struct _httpd_s {
	gand_httpd_param_t param;

	ev_signal sigint;
	ev_signal sighup;
	ev_signal sigterm;
	ev_signal sigpipe;

	struct ev_loop *loop;

	ev_io sock;
};

/* massage the EV macroes a bit */
#undef EV_P
#define EV_P  struct ev_loop *loop __attribute__((unused))


/* socket goodness */
#if !defined MAX_DCCP_CONNECTION_BACK_LOG
# define MAX_DCCP_CONNECTION_BACK_LOG	(5U)
#endif  /* !MAX_DCCP_CONNECTION_BACK_LOG */

static int
make_tcp(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return s;
	}
	/* reuse addr in case we quickly need to turn the server off and on */
	setsock_reuseaddr(s);
	/* turn lingering on */
	setsock_linger(s, 1);
	return s;
}

static int
make_listener(int s, ud_sockaddr_t sa[static 1U])
{
	if (bind(s, &sa->sa, sizeof(*sa)) < 0) {
		return -1;
	}
	return listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
}

static void
log_conn(int fd, ud_sockaddr_t sa)
{
	char abuf[INET6_ADDRSTRLEN];
	short unsigned int p;

	ud_sockaddr_ntop(abuf, sizeof(abuf), sa);
	p = ud_sockaddr_port(sa);
	GAND_INFO_LOG(":sock %d  connection from [%s]:%hu", fd, abuf, p);
	return;
}


/* libev conn handling */
#define MAX_CONNS	(sizeof(free_conns) * 8U)
static uint_fast32_t free_conns = (uint_fast32_t)-1;
static ev_io conns[MAX_CONNS];

static ev_io*
make_io(void)
{

	int c = ffs(free_conns & 0xffffffffU)
		?: ffs(free_conns >> 32U & 0xffffffffU);

	if (LIKELY(c-- > 0)) {
		/* toggle bit in free conns */
		free_conns ^= 1UL << c;
		return conns + c;
	}
	GAND_ERR_LOG("connection pool exhausted");
	return NULL;
}

static void
free_io(ev_io *o)
{
	size_t c = o - conns;

	if (UNLIKELY(c >= MAX_CONNS)) {
		/* huh? */
		return;
	}
	/* toggle C-th bit */
	free_conns ^= 1UL << c;
	memset(o, 0, sizeof(*o));
	return;
}


/* callbacks */
static void
sock_data_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	const int fd = w->fd;

	GAND_INFO_LOG("reading from %d", fd);
	ev_io_stop(EV_A_ w);
	shutdown(fd, SHUT_RDWR);
	close(fd);
	free_io(w);
	return;
}

static void
sock_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ud_sockaddr_t sa;
	socklen_t z = sizeof(sa);
	ev_io *nio;
	int s;

	if ((s = accept(w->fd, &sa.sa, &z)) < 0) {
		GAND_ERR_LOG("connection vanished");
		return;
	}
	log_conn(s, sa);

	if (UNLIKELY((nio = make_io()) == NULL)) {
		GAND_ERR_LOG("too many concurrent connections");
		close(s);
		return;
	}
	ev_io_init(nio, sock_data_cb, s, EV_READ);
	ev_io_start(EV_A_ nio);
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	GAND_NOTI_LOG("C-c caught unrolling everything");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	GAND_NOTI_LOG("SIGHUP caught, doing nothing");
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	GAND_NOTI_LOG("SIGPIPE caught, doing nothing");
	return;
}



gand_httpd_t
make_gand_httpd(const gand_httpd_param_t p)
{
	struct ev_loop *loop = ev_default_loop(EVFLAG_AUTO);
	struct _httpd_s *res = calloc(1, sizeof(*res));

	/* populate public bit */
	res->param = p;

	/* get the socket on the way */
	{
		ud_sockaddr_t addr = {
			.s6.sin6_family = AF_INET6,
			.s6.sin6_addr = in6addr_any,
			.s6.sin6_port = htons(p.port),
		};
		int s;

		if ((s = make_tcp()) < 0) {
			/* big bugger */
			goto foul;
		} else if (make_listener(s, &addr) < 0 ||
			   setsock_rcvtimeo(s, p.timeout) < 0 ||
			   setsock_sndtimeo(s, p.timeout) < 0) {
			/* even bigger bugger */
			close(s);
			goto foul;
		} else {
			socklen_t z = sizeof(addr);

			/* yay */
			ev_io_init(&res->sock, sock_cb, s, EV_READ);
			ev_io_start(EV_A_ &res->sock);

			/* make sure we post back the port number */
                        getsockname(s, &addr.sa, &z);
			res->param.port = ntohs(addr.s6.sin6_port);
		}
	}

	/* initialise private bits */
	ev_signal_init(&res->sigint, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ &res->sigint);
	ev_signal_init(&res->sighup, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ &res->sighup);
	ev_signal_init(&res->sigterm, sigint_cb, SIGTERM);
	ev_signal_start(EV_A_ &res->sigterm);
	ev_signal_init(&res->sigpipe, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ &res->sigpipe);
	res->loop = loop;
	return (gand_httpd_t)res;

foul:
	free(res);
	return NULL;
}

void
free_gand_httpd(gand_httpd_t s)
{
	if (UNLIKELY(s == NULL)) {
		return;
	}
	ev_default_destroy();
	free(s);
	return;
}

void
gand_httpd_run(gand_httpd_t s)
{
	struct _httpd_s *this = (void*)s;

	GAND_NOTI_LOG("httpd ready");
	ev_loop(this->loop, 0);
	GAND_NOTI_LOG("httpd unwound");
	return;
}

/* httpd.c ends here */

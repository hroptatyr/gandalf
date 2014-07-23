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
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
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
	gand_httpd_status_t(*workf)(gand_httpd_req_t);

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


/* our take on memmem() */
static char*
xmemmem(const char *hay, const size_t hz, const char *ndl, const size_t nz)
{
	const char *const eoh = hay + hz;
	const char *const eon = ndl + nz;
	const char *hp;
	const char *np;
	const char *cand;
	unsigned int hsum;
	unsigned int nsum;
	unsigned int eqp;

	/* trivial checks first
	 * a 0-sized needle is defined to be found anywhere in haystack
	 * then run strchr() to find a candidate in HAYSTACK (i.e. a portion
	 * that happens to begin with *NEEDLE) */
	if (nz == 0UL) {
		return deconst(hay);
	} else if ((hay = memchr(hay, *ndl, hz)) == NULL) {
		/* trivial */
		return NULL;
	}

	/* First characters of haystack and needle are the same now. Both are
	 * guaranteed to be at least one character long.  Now computes the sum
	 * of characters values of needle together with the sum of the first
	 * needle_len characters of haystack. */
	for (hp = hay + 1U, np = ndl + 1U, hsum = *hay, nsum = *hay, eqp = 1U;
	     hp < eoh && np < eon;
	     hsum ^= *hp, nsum ^= *np, eqp &= *hp == *np, hp++, np++);

	/* HP now references the (NZ + 1)-th character. */
	if (np < eon) {
		/* haystack is smaller than needle, :O */
		return NULL;
	} else if (eqp) {
		/* found a match */
		return deconst(hay);
	}

	/* now loop through the rest of haystack,
	 * updating the sum iteratively */
	for (cand = hay; hp < eoh; hp++) {
		hsum ^= *cand++;
		hsum ^= *hp;

		/* Since the sum of the characters is already known to be
		 * equal at that point, it is enough to check just NZ - 1
		 * characters for equality,
		 * also CAND is by design < HP, so no need for range checks */
		if (hsum == nsum && memcmp(cand, ndl, nz - 1U) == 0) {
			return deconst(cand);
		}
	}
	return NULL;
}

/* our take on isspace */
static bool
xisspace(int x)
{
	switch (x) {
	default:
		break;
	case ' ':
	case '\r':
	case '\t':
		return true;
	}
	return false;
}


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


/* http goodness */
#include "httpd-verb-gp.c"

static __attribute__((const, pure))  gand_httpd_verb_t
parse_verb(const char *str, size_t len)
{
	const struct httpd_verb_cell_s *const x = __httpd_verb(str, len);

	if (UNLIKELY(x == NULL)) {
		return VERB_UNSUPP;
	}
	return x->verb;
}

static gand_httpd_req_t
parse_hdr(const char *str, size_t len)
{
	gand_httpd_req_t res = {VERB_UNSUPP};
	const char *const ep = str + len;
	char *eox;

	/* guess the verb first */
	if (UNLIKELY((eox = memchr(str, ' ', len)) == NULL)) {
		return res;
	} else if (UNLIKELY(!(res.verb = parse_verb(str, eox - str)))) {
		return res;
	}

	/* nothing much can go wrong now */
	if (UNLIKELY((str = ++eox) >= ep)) {
		/* don't bother */
		return res;
	} else if (UNLIKELY((eox = memchr(str, ' ', ep - str)) == NULL)) {
		/* no path then */
		return res;
	} else if (UNLIKELY(!memcmp(str, "http", 4U) &&
			    (str[4U] == ':' ||
			     str[4U] == 's' && str[5U] == ':'))) {
		char *eoh;

		/* absolute form, read over / and : */
		for (str += 5U; *str == '/' || *str == ':'; str++);
		/* save this reference as host */
		res.host = str;
		/* find the path */
		if (UNLIKELY((eoh = memchr(str, '/', eox - str)) == NULL)) {
			return res;
		}
		/* we need to squeeze a separator between host and path */
		memmove(eoh + 1U, eoh, eox++ - str);
		*eoh++ = '\0';
		res.path = eoh;
	} else {
		char *ho, *eoh;

		/* path is trivial otherwise */
		res.path = str;

		/* but we have to snarf the Host: line */
		if ((ho = xmemmem(eox, ep - eox, "Host:", 5U)) != NULL &&
		    (eoh = memchr(ho, '\n', ep - ho)) != NULL) {
			/* read over leading whitespace */
			for (ho += 5U, eoh--; ho < eoh && xisspace(*ho); ho++);
			/* shrink trailing whitespace */
			for (; eoh > ho && xisspace(eoh[-1]); eoh--);
			/* and assign */
			res.host = ho;
			*eoh = '\0';
		}
	}

	/* demark the end of the request line */
	*eox = '\0';
	/* we may need to split off the query string as well */
	with (char *q) {
		if ((q = memchr(str, '?', eox - str)) != NULL) {
			*q++ = '\0';
			res.query = q;
		}
	}

	/* and finally find beginning of actual headers */
	if (LIKELY((eox = memchr(eox, '\n', ep - eox)) != NULL)) {
		/* overread trailing/leading whitespace */
		for (eox++; xisspace(*eox); eox++);
		res.hdr = (gand_word_t){eox, ep - eox};
	}
	return res;
}

/* auxiliary helpers */
gand_word_t
gand_req_get_xhdr(gand_httpd_req_t req, const char *hdr)
{
	const gand_word_t r = req.hdr;
	char _hdr[64U], *const eo_ = _hdr + countof(_hdr);
	size_t hz;
	const char *h, *eoh;

	with (char *_ = _hdr) {
		for (h = hdr; LIKELY(_ < eo_) && LIKELY((*_ = *h)); _++, h++);
		if (_ >= eo_) {
			goto nul;
		} else if (_ > _hdr && _[-1] != ':') {
			if (UNLIKELY(_ + 1U >= eo_)) {
				/* no room for an additional : */
				goto nul;
			}
			/* otherwise append a colon */
			*_++ = ':';
			*_ = '\0';
		}
		hz = _ - _hdr;
	}

	if (UNLIKELY((h = strstr(r.str, _hdr)) == NULL)) {
		goto nul;
	} else if (UNLIKELY((eoh = memchr(
				     h, '\n', r.len - (h - r.str))) == NULL)) {
		goto nul;
	}
	/* overread leading space */
	for (h += hz; h < eoh && xisspace(*h); h++);
	/* rewind trailing space */
	for (; eoh > h && xisspace(eoh[-1]); eoh--);

	/* that's it, pack up */
	return (gand_word_t){h, eoh - h};

nul:
	return (gand_word_t){NULL};
}

gand_word_t
gand_req_get_xqry(gand_httpd_req_t req, const char *fld)
{
	const char *f;
	const char *eof;

	if (UNLIKELY((f = strstr(req.query, fld)) == NULL)) {
		goto nul;
	} else if ((eof = strchr(f, '&')) == NULL) {
		eof = f + strlen(f);
	}
	return (gand_word_t){f, eof - f};
nul:
	return (gand_word_t){NULL};
}


/* callbacks */
static void
sock_data_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	char buf[4096U];
	const int fd = w->fd;
	ssize_t nrd;
	gand_httpd_req_t req;
	char *eoh;

	if (UNLIKELY((nrd = read(w->fd, buf, sizeof(buf))) <= 0)) {
		goto clo;
	}

	/* parse at least http header boundaries */
	if (UNLIKELY((eoh = xmemmem(buf, nrd, "\r\n\r\n", 4U)) == NULL)) {
		/* header boundary not found, fuck right off */
		goto clo;
	}

	/* now get all them headers parsed */
	*eoh = '\0';
	req = parse_hdr(buf, eoh - buf);

	if (UNLIKELY(req.verb == VERB_UNSUPP)) {
		goto clo;
	}

	with (const struct _httpd_s *h = w->data) {
		switch (h->workf(req)) {
		case STATUS_CLOSE_CONNECTION:
		default:
			goto clo;
		}
	}

clo:
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
	nio->data = w->data;
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
			res->sock.data = res;
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

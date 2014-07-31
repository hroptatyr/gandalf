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
#include <stdio.h>
#include <sys/signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>
#if defined HAVE_SYS_SENDFILE_H
# include <sys/sendfile.h>
#endif	/* HAVE_SYS_SENDFILE_H */
#include <ev.h>
#include "ud-sock.h"
#include "httpd.h"
#include "logger.h"
#include "nifty.h"

/* all the good things we need as context */
struct _httpd_ctx_s {
	gand_httpd_param_t param;

	/* this is the header with the server line appended */
	off_t off_ctyp;
	char proto[256U];

	/* the www directory to serve from */
	char *wwwd;
	size_t wwwz;
};
typedef struct _httpd_ctx_s *restrict _httpd_ctx_t;

/* private version of struct gand_httpd_s */
struct _httpd_s {
	/* the context is the bit we pass around */
	struct _httpd_ctx_s ctx[1U];

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

#if !defined assert
# define assert(x)
#endif	/* !assert */

/* standard header */
static const char min_hdr[] = "\
HTTP/1.1 xxx DESCRIPTION\r\n\
Date: bbb, dd aaa YYYY HH:MM:SS ZZZ\r\n\
Connection: keep-alive\r\n\
Content-Length: 01234567\r\n\
Server: ";
#define OFF_STATUS	sizeof("HTTP/1.1")
#define OFF_DATE	32U
#define OFF_CLEN	103U


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

static size_t
xstrlcpy(char *restrict dst, const char *src, size_t dsz)
{
	size_t ssz = strlen(src);
	if (ssz > dsz) {
		ssz = dsz - 1U;
	}
	memcpy(dst, src, ssz);
	dst[ssz] = '\0';
	return ssz;
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


/* gbuf buffers, at the moment this is just a wrapper around malloc() */
#define MAX_GBUFS	(256U)
#define _X_GBUFS	(32U)
/* roughly a tcp packet minus http header */
#define GBUF_MINZ	(1024U)
static uint32_t used_gbufs[MAX_GBUFS / _X_GBUFS];
static struct gand_gbuf_s {
	unsigned int zbuf;
	unsigned int ibuf;
	uint8_t *data;
} gbufs[MAX_GBUFS];

gand_gbuf_t
make_gand_gbuf(size_t estz)
{
	unsigned int i;
	int k = 0;
	gand_gbuf_t res;

	/* just find us any buffer really */
	for (i = 0U; i < countof(gbufs); i++) {
		if ((k = ffs(~used_gbufs[i]))) {
			goto found;
		}
	}
	return NULL;

found:
	/* toggle bit in used_gbufs */
	k--;
	used_gbufs[i] ^= 1UL << k;
	res = gbufs + (i * _X_GBUFS) + k;

	/* now check if we fulfill the size requirements */
	if (UNLIKELY(estz < GBUF_MINZ)) {
		/* grrr, our users are too indecisive */
		estz = GBUF_MINZ;
	}
	if (res->zbuf < estz) {
		/* realloc to shape */
		res->data = realloc(res->data, res->zbuf = estz);
		if (UNLIKELY(res->data == NULL)) {
			res->zbuf = 0U;
			return NULL;
		}
	}
	return res;
}

void
free_gand_gbuf(gand_gbuf_t gb)
{
	const size_t k = gb - gbufs;

	if (UNLIKELY(k >= countof(gbufs))) {
		/* that's not our buffer */
		GAND_CRIT_LOG("unknown gbuf passed to free_gand_gbuf()");
		return;
	}

	/* reshape buffer */
	if (gb->zbuf == GBUF_MINZ) {
		/* don't worry, keep it at that */
		;
	} else if (gb->zbuf / 2U > gb->ibuf) {
		/* half the size again */
		if (gb->zbuf /= 2U < GBUF_MINZ) {
			gb->zbuf = GBUF_MINZ;
		}
		/* downsize and check */
		gb->data = realloc(gb->data, gb->zbuf);
		if (UNLIKELY(gb->data == NULL)) {
			gb->zbuf = 0U;
		}
	}
	/* reset buffer */
	gb->ibuf = 0U;
	/* toggle used bit again */
	used_gbufs[k / _X_GBUFS] ^= 1UL << (k % _X_GBUFS);
	return;
}

ssize_t
gand_gbuf_write(gand_gbuf_t gb, const void *p, size_t z)
{
/* just like write(3) but to a resizable gbuf */
	if (gb->ibuf + z > gb->zbuf) {
		/* calculate the new size
		 * along with the halving upon free() this
		 * will eventually result in 2-power buffers */
		for (; gb->ibuf + z > gb->zbuf; gb->zbuf *= 2U);
		/* upsize and check */
		gb->data = realloc(gb->data, gb->zbuf);
		if (UNLIKELY(gb->data == NULL)) {
			gb->zbuf = 0U;
			return -1;
		}
	}
	/* and copy we go */
	if (LIKELY(z > 0U)) {
		memcpy(gb->data + gb->ibuf, p, z);
		gb->ibuf += z;
	}
	return z;
}


/* libev conn handling */
#define MAX_CONNS	(sizeof(free_conns) * 8U)
#define MAX_QUEUE	MAX_CONNS
static uint64_t free_conns = -1;
static struct gand_conn_s {
	ev_io r;
	ev_io w;
	unsigned int nwr;
	unsigned int iwr;
	struct gand_wrqi_s {
		gand_httpd_res_t res;
		/* in case of sendfile this is the source socket */
		int fd;
		/* how much have we sent already */
		off_t o;
		/* what's left to transmit */
		size_t z;
	} queue[MAX_QUEUE];
} conns[MAX_CONNS];

static struct gand_conn_s*
make_conn(void)
{
	int i = ffs(free_conns & 0xffffffffU)
		?: ffs(free_conns >> 32U & 0xffffffffU);

	if (LIKELY(i-- > 0)) {
		/* toggle bit in free conns */
		free_conns ^= 1ULL << i;
		return conns + i;
	}
	return NULL;
}

static void
free_conn(struct gand_conn_s *c)
{
	size_t i = c - conns;

	if (UNLIKELY(i >= MAX_CONNS)) {
		/* huh? */
		GAND_CRIT_LOG("unknown connection passed to free_conn()");
		return;
	}
	/* toggle C-th bit */
	free_conns ^= 1ULL << i;
	memset(c, 0, sizeof(*c));
	return;
}

static inline __attribute__((const, pure)) struct gand_wrqi_s*
_top_resp(struct gand_conn_s *c)
{
	if (UNLIKELY(c->nwr == 0U)) {
		return NULL;
	}
	return c->queue + c->iwr;
}

static inline struct gand_wrqi_s*
_bot_resp(struct gand_conn_s *restrict c)
{
	if (UNLIKELY(c->nwr >= MAX_QUEUE)) {
		return NULL;
	}
	return c->queue + (c->iwr + c->nwr) % MAX_QUEUE;
}

static int
_enq_resp(_httpd_ctx_t ctx, struct gand_conn_s *restrict c, gand_httpd_res_t r)
{
	struct gand_wrqi_s *x;

	if (UNLIKELY((x = _bot_resp(c)) == NULL)) {
		return -1;
	}

	switch (r.rd.dtyp) {
		static char *absfn;
		static size_t absfz;
		const char *fn;
		struct stat st;
		int fd;

	default:
	case DTYP_NONE:
		x->z = 0U;
		x->o = 0U;
		x->fd = -1;
		break;

	case DTYP_FILE:
	case DTYP_TMPF:
		if (UNLIKELY((fn = r.rd.file) == NULL)) {
			return -1;
		}
		if (LIKELY(*fn != '/' && ctx->wwwd != NULL)) {
			/* rewrite the filename into something absolute */
			size_t fz = strlen(fn);

			if (UNLIKELY(ctx->wwwz + fz >= absfz)) {
				/* resize */
				size_t nu = ctx->wwwz + fz + 32U;
				absfn = realloc(absfn, nu);
				absfz = nu;
			}
			memcpy(absfn, ctx->wwwd, ctx->wwwz);
			memcpy(absfn + ctx->wwwz, fn, fz);
			absfn[ctx->wwwz + fz] = '\0';
			/* propagate this one as the file name then */
			fn = absfn;
		}
		if (stat(fn, &st) < 0) {
			return -1;
		} else if (st.st_size < 0) {
			return -1;
		} else if ((fd = open(fn, O_RDONLY)) < 0) {
			return -1;
		}
		/* enqueue the request */
		x->z = st.st_size;
		x->o = 0U;
		x->fd = fd;
		break;
	case DTYP_DATA:
		x->z = r.clen;
		x->o = 0U;
		x->fd = -1;
		break;
	case DTYP_GBUF:
		if (LIKELY((x->z = r.clen) == CLEN_UNKNOWN)) {
			/* calculate the size from what's in the gbuf */
			x->z = r.rd.gbuf->ibuf;
		}
		x->o = 0U;
		x->fd = -1;
		break;
	}
	/* assign */
	x->res = r;
	/* and enqueue */
	c->nwr++;
	return 0;
}

static int
_deq_resp(struct gand_conn_s *restrict c)
{
	const struct gand_wrqi_s *x;

	if (UNLIKELY((x = _top_resp(c)) == NULL)) {
		return -1;
	}
	switch (x->res.rd.dtyp) {
	default:
		break;

	case DTYP_TMPF:
		/* remove temporary files */
		(void)unlink(x->res.rd.file);
		/*@fallthrough@*/
	case DTYP_FILE:
		/* close the source descriptor in either case */
		close(x->fd);
		break;

	case DTYP_GBUF:
		/* free gand buffers */
		free_gand_gbuf(x->res.rd.gbuf);
		break;
	}

	/* now actually dequeue */
	if (UNLIKELY(++c->iwr >= MAX_QUEUE)) {
		/* modulo */
		c->iwr = 0U;
	}
	c->nwr--;
	return 0;
}

static int
_deq_conn(struct gand_conn_s *restrict c)
{
/* dequeue everything */
	while (_deq_resp(c) >= 0);
	return 0;
}

static void
shut_conn(struct gand_conn_s *c)
{
/* shuts a connection partially or fully down */
	int fd;

	if (c->r.fd > 0 && c->nwr) {
		/* shut the receiving end but don't close the socket*/
		shutdown(c->r.fd, SHUT_RD);
		c->r.fd = -1;
		return;
	} else if (c->r.fd > 0) {
		/* nothing on the write queue, just shutdown all */
		shutdown(fd = c->r.fd, SHUT_RDWR);
	} else if (c->w.fd > 0) {
		/* read end is already shut */
		shutdown(fd = c->w.fd, SHUT_WR);
		/* dequeue everything */
		_deq_conn(c);
	} else {
		/* all's shut down and closed already, bugger off */
		return;
	}
	close(fd);
	free_conn(c);
	return;
}

static int
_tx_hdr(int fd, _httpd_ctx_t ctx, const struct gand_wrqi_s *x)
{
	struct tm *t;
	time_t now;
	size_t z;

	/* fill in return code */
	snprintf(ctx->proto + OFF_STATUS, 4U, "%3u", x->res.rc);
	ctx->proto[OFF_STATUS + 3U] = ' ';

	/* fill in Date */
	time(&now);
	t = gmtime(&now);
	strftime(ctx->proto + OFF_DATE, 30U, "%b, %d %a %Y %H:%M:%S UTC", t);
	ctx->proto[OFF_DATE + 29U] = '\r';

	/* fill in content length */
	snprintf(ctx->proto + OFF_CLEN, 9U, "%8zu", x->z);
	ctx->proto[OFF_CLEN + 8U] = '\r';

	/* fill in content type */
	z = ctx->off_ctyp;
	z += xstrlcpy(ctx->proto + z, x->res.ctyp, sizeof(ctx->proto) - z);
	ctx->proto[z++] = '\r';
	ctx->proto[z++] = '\n';
	ctx->proto[z++] = '\r';
	ctx->proto[z++] = '\n';

	if (UNLIKELY(send(fd, ctx->proto, z, 0) < (ssize_t)z)) {
		/* oh my god, lucky we didn't send this,
		 * just ask to close the socket */
		return -1;
	}
	return 0;
}

static int
_tx_resp(int fd, _httpd_ctx_t ctx, struct gand_wrqi_s *restrict x)
{
	ssize_t z;

	if (LIKELY(!x->o)) {
		/* cork ... */
		tcp_cork(fd);

		/* ... and send header */
		if (UNLIKELY(_tx_hdr(fd, ctx, x))) {
			/* keep him corked? */
			return -1;
		}
	}

	switch (x->res.rd.dtyp) {
	default:
	case DTYP_NONE:
		/* uncork and send nothing */
		z = 0U;
		break;

	case DTYP_FILE:
	case DTYP_TMPF:
		z = sendfile(fd, x->fd, &x->o, x->z);
		break;
	case DTYP_DATA:
		z = send(fd, x->res.rd.data + x->o, x->z, 0);
		x->o += z;
		break;
	case DTYP_GBUF:
		z = send(fd, x->res.rd.gbuf->data + x->o, x->z, 0);
		x->o += z;
		break;
	}

	tcp_uncork(fd);
	if (UNLIKELY(z < 0)) {
		return -1;
	} else if ((x->z -= z) == 0U) {
		return 1;
	}
	return 0;
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

static void
_build_proto(_httpd_ctx_t ctx, const char *srv)
{
	static const char ct[] = "\r\nContent-Type: ";
	size_t zrv;

	if (srv == NULL) {
		ctx->param.server = srv = PACKAGE_STRING;
	}

	memcpy(ctx->proto, min_hdr, sizeof(min_hdr));
	zrv = xstrlcpy(
		ctx->proto + sizeof(min_hdr) - 1U,
		srv, sizeof(ctx->proto) - sizeof(min_hdr));

	ctx->off_ctyp = sizeof(min_hdr) + zrv - 1U;
	memcpy(ctx->proto + ctx->off_ctyp, ct, sizeof(ct));
	ctx->off_ctyp += sizeof(ct) - 1U;
	return;
}

static void
_build_wwwd(_httpd_ctx_t ctx, const char *wwwd)
{
/* track www directory */
	if (wwwd == NULL) {
		ctx->wwwz = 0U;
		goto out;
	} else if ((ctx->wwwz = strlen(wwwd)) == 0U) {
	out:
		ctx->param.www_dir = "./";
		return;
	}
	/* otherwise bang ... */
	ctx->wwwd = malloc(ctx->wwwz + 1U/*for slash*/ + 1U/*\nul*/);
	memcpy(ctx->wwwd, wwwd, ctx->wwwz);
	if (ctx->wwwd[ctx->wwwz - 1] != '/') {
		ctx->wwwd[ctx->wwwz++] = '/';
	}
	ctx->wwwd[ctx->wwwz] = '\0';

	/* pass back our version of the directory */
	ctx->param.www_dir = ctx->wwwd;
	return;
}


/* callbacks */
static void
sock_resp_cb(EV_P_ ev_io *w, int revents)
{
	struct gand_conn_s *c = (void*)(w - 1U);
	_httpd_ctx_t ctx = w->data;
	struct gand_wrqi_s *x;

	if (UNLIKELY(!(revents & EV_WRITE))) {
		/* oh big cluster fuck */
		GAND_CRIT_LOG("responder can't write to socket");
		shut_conn(c);
		goto clo;
	}

	/* pop item from queue */
	if (LIKELY((x = _top_resp(c)) != NULL)) {
		if (_tx_resp(c->w.fd, ctx, x)) {
			/* -1 indicates error, 1 indicates complete
			 * in either case dequeue the write queue item */
			_deq_resp(c);
		}
	}

	if (_top_resp(c) == NULL) {
	clo:
		/* finished writing */
		ev_io_stop(EV_A_ w);
	}
	return;
}

static void
sock_data_cb(EV_P_ ev_io *w, int revents)
{
	char buf[4096U];
	const int fd = w->fd;
	_httpd_ctx_t ctx = w->data;
	ssize_t nrd;
	gand_httpd_req_t req;
	char *eoh;

	if (UNLIKELY(!(revents & EV_READ))) {
		/* huh? */
		goto clo;
	}

	/* read some data into our tiny buffer */
	if (UNLIKELY((nrd = read(fd, buf, sizeof(buf))) <= 0)) {
		/* EOF or some other failure */
		goto clo;
	}

	/* parse at least http header boundaries */
	if (UNLIKELY((eoh = xmemmem(buf, nrd, "\r\n\r\n", 4U)) == NULL)) {
		/* header boundary not found, fuck right off */
		goto clo;
	}

	/* now get all them headers parsed */
	eoh[2U] = '\0';
	req = parse_hdr(buf, eoh + 2U - buf);

	if (UNLIKELY(req.verb == VERB_UNSUPP)) {
		/* don't deal with deliquents, we speak HTTP/1.1 only */
		goto clo;
	}

	with (gand_httpd_res_t(*workf)() = ctx->param.workf) {
		gand_httpd_res_t res = workf(req);
		struct gand_conn_s *c = (void*)w;

		if (c->w.fd <= 0) {
			/* initialise write watcher */
			c->w.data = ctx;
			ev_io_init(&c->w, sock_resp_cb, fd, EV_WRITE);
			assert(c->nwr == 0U);
		}
		if (LIKELY(!c->nwr)) {
			/* restart the write watcher */
			ev_io_start(EV_A_ &c->w);
		} else if (UNLIKELY(c->nwr >= MAX_QUEUE)) {
			GAND_ERR_LOG("transmission queue for socket %d full",
				     c->w.fd);
			goto clo;
		} else {
			/* already started it seems */
			assert(c->w.fd > 0);
		}

		/* enqueue the request */
		if (UNLIKELY(_enq_resp(ctx, c, res) < 0)) {
			/* fuck */
			GAND_ERR_LOG("cannot enqueue response for %d", c->w.fd);
			goto clo;
		}
	}
	return;

clo:
	ev_io_stop(EV_A_ w);
	shut_conn((struct gand_conn_s*)w);
	return;
}

static void
sock_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ud_sockaddr_t sa;
	socklen_t z = sizeof(sa);
	struct gand_conn_s *nio;
	int s;

	if ((s = accept(w->fd, &sa.sa, &z)) < 0) {
		GAND_ERR_LOG("connection vanished");
		return;
	}
	log_conn(s, sa);

	if (UNLIKELY((nio = make_conn()) == NULL)) {
		GAND_ERR_LOG("too many concurrent connections");
		close(s);
		return;
	}
	with (_httpd_ctx_t ctx = w->data) {
		/* pass on the httpd context then */
		nio->r.data = ctx;
	}
	ev_io_init(&nio->r, sock_data_cb, s, EV_READ);
	ev_io_start(EV_A_ &nio->r);
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
	GAND_NOTI_LOG("SIGPIPE caught, checking connections ...");

	/* check for half-open shit */
	for (size_t i = 0U; i < countof(conns); i++) {
		if (conns[i].r.fd < 0 && conns[i].w.fd > 0 &&
		    conns[i].iwr > conns[i].nwr) {
			GAND_INFO_LOG("connection %zu seems buggered", i);
			ev_io_stop(EV_A_ &conns[i].w);
			shut_conn(conns + i);
		} else if (conns[i].r.fd < 0 && conns[i].w.fd < 0) {
			/* is this possible? */
			;
		}
	}
	return;
}



gand_httpd_t
make_gand_httpd(const gand_httpd_param_t p)
{
	struct ev_loop *loop = ev_default_loop(EVFLAG_AUTO);
	struct _httpd_s *res = calloc(1, sizeof(*res));

	/* populate public bit */
	res->ctx->param = p;

	/* get the socket on the way */
	{
		ud_sockaddr_t addr = {
			.s6.sin6_family = AF_INET6,
			.s6.sin6_addr = in6addr_any,
			.s6.sin6_port = htons(p.port),
		};
		int s;

		if (UNLIKELY((s = make_tcp()) < 0)) {
			/* big bugger */
			GAND_ERR_LOG("cannot obtain a tcp socket: %s",
				     strerror(errno));
			goto foul;
		} else if (make_listener(s, &addr) < 0 ||
			   setsock_rcvtimeo(s, p.timeout) < 0 ||
			   setsock_sndtimeo(s, p.timeout) < 0) {
			GAND_ERR_LOG("cannot listen on tcp socket: %s",
				     strerror(errno));
			/* even bigger bugger */
			close(s);
			goto foul;
		} else {
			socklen_t z = sizeof(addr);

			/* yay */
			res->sock.data = res->ctx;
			ev_io_init(&res->sock, sock_cb, s, EV_READ);
			ev_io_start(EV_A_ &res->sock);

			/* make sure we post back the port number */
                        getsockname(s, &addr.sa, &z);
			res->ctx->param.port = ntohs(addr.s6.sin6_port);
		}
	}

	/* get the proto buffer ready */
	_build_proto(res->ctx, p.server);
	_build_wwwd(res->ctx, p.www_dir);

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
	with (struct _httpd_s *_ = (void*)s) {
		if (_->ctx->wwwd != NULL) {
			free(_->ctx->wwwd);
		}
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

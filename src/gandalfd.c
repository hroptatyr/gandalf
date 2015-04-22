/*** gandalfd.c -- rolf and milf accessor
 *
 * Copyright (C) 2011-2014 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of the army of unserding daemons.
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
#if defined HAVE_VERSION_H
# include "version.h"
#endif	/* HAVE_VERSION_H */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#if defined HAVE_EV_H
# include <ev.h>
#endif	/* HAVE_EV_H */
#include "httpd.h"
#include "gand-dict.h"
#include "gand-cfg.h"
#include "logger.h"
#include "fops.h"
#include "nifty.h"

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

#undef EV_P
#define EV_P  struct ev_loop *loop __attribute__((unused))

typedef struct {
	const char *s;
	size_t z;
} word_t;

struct rln_s {
	word_t sym;
	word_t dat;
	word_t vrb;
	word_t val;
};

static dict_t gsymdb;
static int trolf_dirfd;


static void
block_sigs(void)
{
	sigset_t fatal_signal_set[1];

	sigemptyset(fatal_signal_set);
	sigaddset(fatal_signal_set, SIGHUP);
	sigaddset(fatal_signal_set, SIGQUIT);
	sigaddset(fatal_signal_set, SIGINT);
	sigaddset(fatal_signal_set, SIGTERM);
	sigaddset(fatal_signal_set, SIGXCPU);
	sigaddset(fatal_signal_set, SIGXFSZ);
	(void)sigprocmask(SIG_BLOCK, fatal_signal_set, (sigset_t*)NULL);
	return;
}

static void
unblock_sigs(void)
{
	sigset_t empty_signal_set[1];

	sigemptyset(empty_signal_set);
	sigprocmask(SIG_SETMASK, empty_signal_set, (sigset_t*)NULL);
	return;
}

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


static const char*
make_lateglu_name(dict_oid_t rid)
{
	static char f[PATH_MAX] = "show_lateglu/";
	int x;

	x = snprintf(f + 13U, sizeof(f) - 13U, "%04u/%08u", rid / 10000U, rid);
	if (UNLIKELY(x < 0 || (size_t)x >= sizeof(f) - 13U)) {
		return NULL;
	}
	return f;
}

static __attribute__((unused)) const char*
make_super_name(dict_oid_t rid)
{
	static char f[PATH_MAX] = "super/";
	int x;

	x = snprintf(f + 6U, sizeof(f) - 6U, "%04u/%08u", rid / 10000U, rid);
	if (UNLIKELY(x < 0 || (size_t)x >= sizeof(f) - 6U)) {
		return NULL;
	}
	return f;
}

static struct rln_s
snarf_rln(const char *ln, size_t lz)
{
	struct rln_s r;
	const char *p;

	/* normally first up is the rolf-id, overread him */
	if (UNLIKELY((p = memchr(ln, '\t', lz)) == NULL)) {
		goto b0rk;
	}

	/* snarf sym */
	r.sym.s = ++p;
	/* find separator between symbol and trans-id */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}
	r.sym.z = p++ - r.sym.s;

	/* find separator between trans-id and date stamp */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}

	/* snarf date */
	r.dat.s = ++p;
	/* find separator between date stamp and valflav aka verb */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}
	r.dat.z = p++ - r.dat.s;

	/* snarf valflav aka verb */
	r.vrb.s = p;
	/* find separator between date stamp and valflav aka verb */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}
	r.vrb.z = p++ - r.vrb.s;

	/* snarf value */
	r.val.s = p;
	r.val.z = ln + lz - p;

	/* that's all */
	return r;

b0rk:
	return (struct rln_s){NULL};
}


/* filter routines */
typedef word_t flt_t;
static const flt_t nul_flt;

#define FILTER_LAST_INDICATOR	((const void*)0xdeadU)
#define FILTER_FRST_INDICATOR	((const void*)0xcafeU)
static const struct rln_s FILTER_FRST = {
	.sym = {.s = FILTER_FRST_INDICATOR},
};
static const struct rln_s FILTER_LAST = {
	.sym = {.s = FILTER_LAST_INDICATOR},
};

static bool
flt_matches_p(flt_t f, word_t w)
{
	if (f.s == NULL) {
		/* everything matches the trivial filter */
		return true;
	} else if (f.z == 0U) {
		/* nothing matches the other trivial filter */
		return false;
	}
	/* otherwise it's a non-trivial filter  */
	for (const char *fp = f.s, *const ef = f.s + f.z;
	     fp < ef && (fp = xmemmem(fp, ef - fp, w.s, w.z)) != NULL; fp++) {
		/* check word boundary */
		if (fp[-1] == '\0' && fp[w.z] == '\0') {
			/* perfick */
			return true;
		}
	}
	return false;
}

static ssize_t
filter_csv(char *restrict scratch, size_t z, struct rln_s r, flt_t f)
{
	char *sp = scratch;

	if (0) {
		;
	} else if (UNLIKELY(r.sym.s == FILTER_FRST_INDICATOR)) {
		return 0;
	} else if (UNLIKELY(r.sym.s == FILTER_LAST_INDICATOR)) {
		return 0;
	}

	/* quick sanity check, should also warm up caches */
	if (UNLIKELY(r.sym.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.dat.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.vrb.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.val.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.sym.z + 1U/*\t*/ +
			    r.dat.z + 1U/*\t*/ +
			    r.vrb.z + 1U/*\t*/ +
			    r.val.z + 1U/*\n*/ >= z)) {
		/* request a bigger buffer */
		return -2;
	}

	/* do some filtering here as well,
	 * this could be split off into the filtering coroutine really */
	if (!flt_matches_p(f, r.vrb)) {
		/* bugger off before anything else */
		return 0;
	}

	/* now copy over sym, dat, vrb and val */
	memcpy(sp, r.sym.s, r.sym.z);
	sp += r.sym.z;
	*sp++ = '\t';

	memcpy(sp, r.dat.s, r.dat.z);
	sp += r.dat.z;
	*sp++ = '\t';

	memcpy(sp, r.vrb.s, r.vrb.z);
	sp += r.vrb.z;
	*sp++ = '\t';

	memcpy(sp, r.val.s, r.val.z);
	sp += r.val.z;
	*sp++ = '\n';

	return sp - scratch;
}

static ssize_t
filter_json(char *restrict scratch, size_t z, struct rln_s r, flt_t f)
{
	static struct rln_s prev;
	char *restrict sp = scratch;
	size_t spc_needed = 0U;
	unsigned int flags = 0U;

	if (0) {
		;
	} else if (UNLIKELY(r.sym.s == FILTER_FRST_INDICATOR)) {
		*sp++ = '[';
		return sp - scratch;
	} else if (UNLIKELY(r.sym.s == FILTER_LAST_INDICATOR)) {
		if (LIKELY(prev.sym.s != NULL)) {
			if (UNLIKELY(z < 10U)) {
				return -2;
			}
			/* clear out prev */
			memset(&prev, 0, sizeof(prev));
			/* finalise dat indentation */
			*sp++ = '\n';
			*sp++ = ' ';
			*sp++ = ' ';
			*sp++ = ']';
			*sp++ = '}';
			*sp++ = '\n';
			/* finalise sym */
			*sp++ = ']';
			*sp++ = '}';
		}
		if (UNLIKELY(z < 2U)) {
			return -2;
		}
		/* display this in either case */
		*sp++ = ']';
		*sp++ = '\n';
		return sp - scratch;
	}

	if (UNLIKELY(r.sym.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.dat.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.vrb.s == NULL)) {
		return -1;
	} else if (UNLIKELY(r.val.s == NULL)) {
		return -1;
	}

	/* do some filtering here as well,
	 * this could be split off into the filtering coroutine really */
	if (!flt_matches_p(f, r.vrb)) {
		/* bugger off before anything else */
		return 0;
	}

	if (prev.sym.s == NULL || memcmp(prev.sym.s, r.sym.s, r.sym.z)) {
		spc_needed += r.sym.z + 2U/*quot*/ + sizeof("[{\"sym\":}]") +
			sizeof("[\"data:]\"");
		flags |= 0b01U;
	}

	if (prev.dat.s == NULL || memcmp(prev.dat.s, r.dat.s, r.dat.z)) {
		spc_needed += r.dat.z + 2U/*quot*/ + sizeof("  {\"dat\":[]}") +
			sizeof("[\"data:]\"");
		flags |= 0b10U;
	}

	/* definitely need the verb and value space */
	spc_needed += r.vrb.z + 2U/*quot*/ + sizeof("    {\"vrb\":}");
	spc_needed += r.val.z + 2U/*quot*/ + sizeof("    {\"val\":}");

	if (UNLIKELY(spc_needed + 3U/*separators*/ >= z)) {
		/* request a bigger buffer */
		return -2;
	}

#define LITCPY(x, lit)	(memcpy(x, lit, sizeof(lit) - 1U), sizeof(lit) - 1U)
#define BUFCPY(x, d, z)	(memcpy(x, d, z), z)
	if (flags & 0b01U) {
		/* copy symbol */
		*sp++ = '{';
		sp += LITCPY(sp, "\"sym\":");
		*sp++ = '"';
		sp += BUFCPY(sp, r.sym.s, r.sym.z);
		*sp++ = '"';
		*sp++ = ',';
		sp += LITCPY(sp, "\"data\":[\n");
	}

	if (flags & 0b10U) {
		if (prev.dat.s) {
			*sp++ = '\n';
			*sp++ = ' ';
			*sp++ = ' ';
			*sp++ = ']';
			*sp++ = '}';
			*sp++ = ',';
			*sp++ = '\n';
		}
		/* copy date */
		*sp++ = ' ';
		*sp++ = ' ';
		*sp++ = '{';
		sp += LITCPY(sp, "\"dat\":");
		*sp++ = '"';
		sp += BUFCPY(sp, r.dat.s, r.dat.z);
		*sp++ = '"';
		*sp++ = ',';
		sp += LITCPY(sp, "\"data\":[");
	}

	/* now copy over vrb/val pairs */
	if (!(flags & 0b10U)) {
		*sp++ = ',';
	}	*sp++ = '\n';

	*sp++ = ' ';
	*sp++ = ' ';
	*sp++ = ' ';
	*sp++ = ' ';
	*sp++ = '{';
	sp += LITCPY(sp, "\"vrb\":");
	*sp++ = '"';
	sp += BUFCPY(sp, r.vrb.s, r.vrb.z);
	*sp++ = '"';
	*sp++ = '}';
	*sp++ = ',';
	*sp++ = '{';
	sp += LITCPY(sp, "\"value\":");
	*sp++ = '"';
	sp += BUFCPY(sp, r.val.s, r.val.z);
	*sp++ = '"';
	*sp++ = '}';

	/* keep a note about this line */
	prev = r;
	return sp - scratch;
}


/* rolf <-> onion glue */
typedef enum {
	EP_UNK,
	EP_V0_SERIES,
	EP_V0_SOURCES,
	EP_V0_FILES,
	EP_V0_MAIN,
} gand_ep_t;

typedef enum {
	OF_UNK,
	OF_CSV,
	OF_JSON,
	OF_HTML,
} gand_of_t;

static const char _ofs_UNK[] = "text/plain";
static const char _ofs_JSON[] = "application/json";
static const char _ofs_CSV[] = "text/csv";
static const char _ofs_HTML[] = "text/html";
#define OF(_x_)		(_ofs_ ## _x_)

static const char *const _ofs[] = {
	[OF_UNK] = OF(UNK),
	[OF_JSON] = OF(JSON),
	[OF_CSV] = OF(CSV),
	[OF_HTML] = OF(HTML),
};

static const char _eps_V0_SERIES[] = "/v0/series";
static const char _eps_V0_SOURCES[] = "/v0/sources";
static const char _eps_V0_FILES[] = "/v0/files";
#define EP(_x_)		(_eps_ ## _x_)

static gand_ep_t
__gand_ep(const char *s, size_t z)
{
/* perform a prefix match */

	if (z < sizeof(EP(V0_SERIES)) - 1U) {
		;
	} else if (!memcmp(EP(V0_SERIES), s, sizeof(EP(V0_SERIES)) - 1U)) {
		return EP_V0_SERIES;
	} else if (z < sizeof(EP(V0_SOURCES)) - 1U) {
		;
	} else if (!memcmp(EP(V0_SOURCES), s, sizeof(EP(V0_SOURCES)) - 1U)) {
		return EP_V0_SOURCES;
	} else if (z < sizeof(EP(V0_FILES)) - 1U) {
		;
	} else if (!memcmp(EP(V0_FILES), s, sizeof(EP(V0_FILES)) - 1U)) {
		return EP_V0_FILES;
	}
	return EP_UNK;
}

static gand_of_t
__gand_of(const char *s, size_t z)
{
	if (z < sizeof(OF(CSV)) - 1U) {
		;
	} else if (!memcmp(OF(CSV), s, sizeof(OF(CSV)) - 1U)) {
		return OF_CSV;
	} else if (z < sizeof(OF(JSON)) - 1U) {
		;
	} else if (!memcmp(OF(JSON), s, sizeof(OF(JSON)) - 1U)) {
		return OF_JSON;
	}
	return OF_UNK;
}

static __attribute__((const, pure)) gand_ep_t
req_get_endpoint(gand_httpd_req_t req)
{
	const char *cmd;
	size_t cmz;

	if (UNLIKELY((cmd = req.path) == NULL)) {
		return EP_UNK;
	} else if (UNLIKELY((cmz = strlen(cmd)) == 0U)) {
		return EP_UNK;
	} else if (UNLIKELY(cmz == 1U && *cmd == '/')) {
		return EP_V0_MAIN;
	}
	return __gand_ep(cmd, cmz);
}

static gand_of_t
req_get_outfmt(gand_httpd_req_t req)
{
	gand_word_t acc = gand_req_get_xhdr(req, "Accept");
	gand_of_t of = OF_UNK;
	const char *on;

	if (UNLIKELY(acc.str == NULL)) {
		return OF_UNK;
	}
	/* otherwise */
	do {
		if ((on = strchr(acc.str, ',')) != NULL) {
			acc.len = on++ - acc.str;
		}
		/* check if there's semicolon specs */
		with (const char *sc = strchr(acc.str, ';')) {
			if (sc && on && sc < on || sc) {
				acc.len = sc - acc.str;
			}
		}

		if (LIKELY((of = __gand_of(acc.str, acc.len)) != OF_UNK)) {
			/* first one wins */
			break;
		}
	} while ((acc.str = on));
	return of;
}

static flt_t
ser_get_filter(gand_httpd_req_t r)
{
	static const char Qf[] = "filter";
	static char _f[256U];
	gand_word_t w;

	if ((w = gand_req_get_xqry(r, Qf)).str == NULL) {
		/* just go with the flow */
		return (flt_t){NULL};
	} else if ((w.str += sizeof(Qf), w.len -= sizeof(Qf), false)) {
		/* not reached */
		;
	} else if (w.len > sizeof(_f) - 2U) {
		GAND_ERR_LOG("filter string too long, truncating?");
		w.len = sizeof(_f) - 2U;
	}
	_f[0U] = '\0';
	memcpy(_f + 1U, w.str, w.len);
	_f[1U + w.len] = '\0';
	/* massage the filter string a little */
	for (char *fp = _f + 1U, *const ef = fp + w.len; fp < ef; fp++) {
		if (*fp == ',') {
			*fp = '\0';
		}
	}
	return (flt_t){_f, w.len + 2U};
}

static void
subst_rln(struct rln_s *restrict r, const char *host)
{
	static char *scratch;
	static size_t zcratch;
	static size_t scroff;

	if (UNLIKELY(host == NULL)) {
		/* reset request */
		scroff = 0U;
		return;
	}
	if (UNLIKELY(!scroff)) {
		/* construct URI prefix */
		size_t hlen = strlen(host);

		if (7U + hlen + sizeof(_eps_V0_FILES) + r->val.z > zcratch) {
			zcratch = 7U + hlen + sizeof(_eps_V0_FILES) + r->val.z;
			zcratch += 64U;
			zcratch &= ~(64U - 1U);

			scratch = realloc(scratch, zcratch);
		}
		/* bang http:// */
		memcpy(scratch, "http://", 7U);
		scroff = 7U;
		memcpy(scratch + scroff, host, hlen);
		scroff += hlen;
		memcpy(scratch + scroff, _eps_V0_FILES, sizeof(_eps_V0_FILES));
		scroff += sizeof(_eps_V0_FILES) - 1U;
	}
	if (scroff + r->val.z > zcratch) {
		zcratch = scroff + r->val.z;
		zcratch += 64U;
		zcratch &= ~(64U - 1U);

		scratch = realloc(scratch, zcratch);
	}
	/* now actually subst file:// */
	memcpy(scratch + scroff, r->val.s + 6U, r->val.z - 6U);
	r->val.s = scratch;
	r->val.z += scroff - 6U;
	scratch[r->val.z] = '\0';
	return;
}

static gand_httpd_res_t
work_ser(gand_httpd_req_t req)
{
	char buf[4096U];
	const char *sym;
	dict_oid_t rid;
	gand_of_t of;
	const char *fn;
	gandfn_t fx;
	ssize_t(*filter)(char *restrict, size_t, struct rln_s ln, flt_t f);
	gand_gbuf_t gb;
	flt_t f;

	if ((of = req_get_outfmt(req)) == OF_UNK) {
		of = OF_CSV;
	}
	if ((sym = req.path + sizeof(EP(V0_SERIES)))[-1] != '/') {
		static const char errmsg[] = "Bad Request\n";

		GAND_INFO_LOG(":rsp [400 Bad request]");
		return (gand_httpd_res_t){
			.rc = 400U/*BAD REQUEST*/,
			.ctyp = OF(UNK),
			.clen = sizeof(errmsg)- 1U,
			.rd = {DTYP_DATA, GAND_RES_DATA(data) = errmsg},
		};
	} else if (!(rid = dict_get_sym(gsymdb, sym))) {
		static const char errmsg[] = "Symbol not found\n";

		GAND_INFO_LOG(":rsp [409 Conflict]: Symbol not found");
		return (gand_httpd_res_t){
			.rc = 409U/*CONFLICT*/,
			.ctyp = OF(UNK),
			.clen = sizeof(errmsg)- 1U,
			.rd = {DTYP_DATA, GAND_RES_DATA(data) = errmsg},
		};
	}

	/* otherwise we've got some real yacka to do */
	if (UNLIKELY((fn = make_lateglu_name(rid)) == NULL)) {
		goto interr;
	} else if ((fx = mmapat_fn(trolf_dirfd, fn, O_RDONLY)).fd < 0) {
		goto interr;
	}

	/* obtain the filter */
	f = ser_get_filter(req);

	switch (of) {
	default:
	case OF_CSV:
		of = OF_CSV;
		filter = filter_csv;
		break;
	case OF_JSON:
		filter = filter_json;
		break;
	}

	/* obtain the buffer we can send bytes to */
	const gandf_t fb = fx.fb;
	size_t tot = 0U;
	if (UNLIKELY((gb = make_gand_gbuf(fb.z)) == NULL)) {
		GAND_ERR_LOG("cannot obtain gbuf");
		goto interr_unmap;
	}

	/* traverse the lines, filter and rewrite them */
	with (ssize_t z) {
	bfilt:
		z = filter(buf + tot, sizeof(buf) - tot, FILTER_FRST, nul_flt);

		if (z < -1) {
			/* flush buffer */
			if (UNLIKELY(gand_gbuf_write(gb, buf, tot) < 0)) {
				GAND_ERR_LOG("cannot write to gbuf");
				goto interr_unmap_unbuf;
			}
			tot = 0U;
			goto bfilt;
		} else if (z >= 0) {
			tot += z;
		}
	}
	for (size_t i = 0U; i < fb.z; i++) {
		const char *const bol = (const char*)fb.d + i;
		const size_t max = fb.z - i;
		const char *eol;
		struct rln_s ln;
		ssize_t z;

		if (UNLIKELY((eol = memchr(bol, '\n', max)) == NULL)) {
			eol = bol + max;
		}
		/* snarf the line, v0 format, zero copy */
		ln = snarf_rln(bol, eol - bol);

		if (UNLIKELY(ln.val.z > 7U &&
			     !memcmp(ln.val.s, "file://", 7U))) {
			subst_rln(&ln, req.host);
		}
	filt:
		/* filter, maybe */
		z = filter(buf + tot, sizeof(buf) - tot, ln, f);
		if (UNLIKELY(z < -1)) {
			/* flush buffer */
			if (UNLIKELY(gand_gbuf_write(gb, buf, tot) < 0)) {
				GAND_ERR_LOG("cannot write to gbuf");
				goto interr_unmap_unbuf;
			}
			tot = 0U;
			goto filt;
		} else if (UNLIKELY(z < 0)) {
			/* `just' an error */
			goto next;
		}
		tot += z;
	next:
		i += eol - bol;
	}
	/* flush filter */
	with (ssize_t z) {
	efilt:
		z = filter(buf + tot, sizeof(buf) - tot, FILTER_LAST, nul_flt);

		if (z < -1) {
			/* flush buffer */
			if (UNLIKELY(gand_gbuf_write(gb, buf, tot) < 0)) {
				GAND_ERR_LOG("cannot write to gbuf");
				goto interr_unmap_unbuf;
			}
			tot = 0U;
			goto efilt;
		} else if (z >= 0) {
			tot += z;
		}
	}
	/* reset subst'er */
	subst_rln(NULL, NULL);

	/* flush */
	if (UNLIKELY(gand_gbuf_write(gb, buf, tot) < 0)) {
		GAND_ERR_LOG("cannot write to gbuf");
		goto interr_unmap_unbuf;
	}

	munmap_fn(fx);
	GAND_INFO_LOG(":rsp [200 OK]: series %08u", rid);
	return (gand_httpd_res_t){
		.rc = 200U/*OK*/,
		.ctyp = _ofs[of],
		.clen = CLEN_UNKNOWN,
		.rd = {DTYP_GBUF, GAND_RES_DATA(gbuf) = gb},
	};

interr_unmap_unbuf:
	free_gand_gbuf(gb);
interr_unmap:
	munmap_fn(fx);
interr:
	GAND_INFO_LOG(":rsp [500 Internal Error]");
	return (gand_httpd_res_t){
		.rc = 500U/*INTERNAL ERROR*/,
		.ctyp = OF(UNK),
		.clen = 0U,
		.rd = {DTYP_NONE},
	};
}

static gand_httpd_res_t
work_fil(gand_httpd_req_t req)
{
	const char *fn;
	int fd;

	if ((fn = req.path + sizeof(EP(V0_FILES)))[-1] != '/') {
		goto r400;

	} else if (*fn == '.' || *fn == '/') {
		/* don't let them trick us into leaving trolf_dirfd
		 * by using relative paths */
		goto r400;

	} else if (UNLIKELY((fd = openat(trolf_dirfd, fn, O_RDONLY)) < 0)) {
		goto r409;
	}

	/* success */
	return (gand_httpd_res_t){
		.rc = 200U/*OK*/,
		.ctyp = OF(UNK),
		.rd = {DTYP_SOCK, GAND_RES_DATA(sock) = fd},
	};

r400:
	GAND_INFO_LOG(":rsp [400 Bad request]");
	static const char err400[] = "Bad Request\n";
	return (gand_httpd_res_t){
		.rc = 400U/*BAD REQUEST*/,
		.ctyp = OF(UNK),
		.clen = sizeof(err400)- 1U,
		.rd = {DTYP_DATA, GAND_RES_DATA(data) = err400},
	};

r409:
	GAND_INFO_LOG(":rsp [409 Conflict]: File not found");
	static const char err409[] = "File not found\n";
	return (gand_httpd_res_t){
		.rc = 409U/*CONFLICT*/,
		.ctyp = OF(UNK),
		.clen = sizeof(err409)- 1U,
		.rd = {DTYP_DATA, GAND_RES_DATA(data) = err409},
	};
}

static gand_httpd_res_t
work_src(gand_httpd_req_t req)
{
	static const char _s[] = "rolf_source";
	gandfn_t fb;
	const char *src;
	gand_of_t of;
	gand_gbuf_t gb;

	if ((of = req_get_outfmt(req)) == OF_UNK) {
		of = OF_CSV;
	}
	if ((src = req.path + sizeof(EP(V0_SOURCES)))[-1] == '\0' ||
	    *src == '\0') {
		/* they just want all sources listed */
		goto all;
	} else if (src[-1] != '/') {
		static const char errmsg[] = "Bad Request\n";

		GAND_INFO_LOG(":rsp [400 Bad request]");
		return (gand_httpd_res_t){
			.rc = 400U/*BAD REQUEST*/,
			.ctyp = OF(UNK),
			.clen = sizeof(errmsg)- 1U,
			.rd = {DTYP_DATA, GAND_RES_DATA(data) = errmsg},
		};
	}

	/* obtain the buffer we can send bytes to */
	if (UNLIKELY((gb = make_gand_gbuf(4096U)) == NULL)) {
		GAND_ERR_LOG("cannot obtain gbuf");
		goto interr_unmap;
	}
	for (dict_si_t si; (si = dict_src_iter(gsymdb, src)).sid;) {
		char sym[256U];
		size_t len = strlen(si.sym);

		if (len >= sizeof(sym)) {
			/* buffer oflow protection */
			len = sizeof(sym) - 1U;
		}
		memcpy(sym, si.sym, len);
		sym[len++] = '\n';
		gand_gbuf_write(gb, sym, len);
	}

	GAND_INFO_LOG(":rsp [200 OK]: source %s", src);
	return (gand_httpd_res_t){
		.rc = 200U/*OK*/,
		.ctyp = _ofs[of],
		.clen = CLEN_UNKNOWN,
		.rd = {DTYP_GBUF, GAND_RES_DATA(gbuf) = gb},
	};

all:
	if (UNLIKELY((fb = mmapat_fn(trolf_dirfd, _s, O_RDONLY)).fd < 0)) {
		/* big fuck */
		goto interr;
	}

	/* traverse the lines, filter and rewrite them */
	const char *const buf = fb.fb.d;
	const size_t bsz = fb.fb.z;
	char proto[4096U];
	size_t tot = 0U;

	/* obtain the buffer we can send bytes to */
	if (UNLIKELY((gb = make_gand_gbuf(bsz)) == NULL)) {
		GAND_ERR_LOG("cannot obtain gbuf");
		goto interr_unmap;
	}

	for (size_t i = 0U; i < bsz; i++) {
		const char *const bol = buf + i;
		const size_t max = bsz - i;
		const char *eol;
		const char *ln;

		if (UNLIKELY((eol = memchr(bol, '\n', max)) == NULL)) {
			eol = bol + max;
		}
		if (LIKELY((ln = memchr(bol, '\t', eol - bol)) != NULL)) {
			/* step behind the tab separator */
			ln++;

			if (UNLIKELY(tot + (eol - ln) > sizeof(proto))) {
				gand_gbuf_write(gb, proto, tot);
				tot = 0U;
			}

			memcpy(proto + tot, ln, eol - ln);
			tot += eol - ln;
			proto[tot++] = '\n';
			i += eol - bol;
		}
	}
	/* flush */
	if (UNLIKELY(gand_gbuf_write(gb, proto, tot) < 0)) {
		goto interr_unmap;
	}

	munmap_fn(fb);
	GAND_INFO_LOG(":rsp [200 OK]: all sources");
	return (gand_httpd_res_t){
		.rc = 200U/*OK*/,
		.ctyp = _ofs[of],
		.clen = CLEN_UNKNOWN,
		.rd = {DTYP_GBUF, GAND_RES_DATA(gbuf) = gb},
	};

interr_unmap:
	munmap_fn(fb);
interr:
	GAND_INFO_LOG(":rsp [500 Internal Error]");
	return (gand_httpd_res_t){
		.rc = 500U/*INTERNAL ERROR*/,
		.ctyp = OF(UNK),
		.clen = 0U,
		.rd = {DTYP_NONE},
	};
}

static gand_httpd_res_t
work(gand_httpd_req_t req)
{
	static const char *const v[NVERBS] = {
		[VERB_GET] = "GET",
		[VERB_POST] = "POST",
		[VERB_PUT] = "PUT",
		[VERB_DELETE] = "DELETE",
	};
	GAND_INFO_LOG(":req [%s http://%s%s?%s]",
		      v[req.verb] ?: "UNK",
		      req.host ?: "",
		      req.path ?: "/",
		      req.query ?: "");

	/* split by endpoint */
	switch (req_get_endpoint(req)) {
	case EP_V0_SERIES:
		return work_ser(req);
	case EP_V0_SOURCES:
		return work_src(req);
	case EP_V0_FILES:
		return work_fil(req);
	case EP_V0_MAIN:
		GAND_INFO_LOG(":rsp [200 OK]");
		return (gand_httpd_res_t){
			.rc = 200U/*OK*/,
			.ctyp = "text/html",
			.clen = CLEN_UNKNOWN,
			.rd = {DTYP_FILE, GAND_RES_DATA(file) = "404.html"},
		};

	default:
	case EP_UNK:
		break;
	}

	/* unsure what to do */
	GAND_INFO_LOG(":rsp [404 Not Found]");
	return (gand_httpd_res_t){
		.rc = 404U/*NOT FOUND*/,
		.ctyp = "text/html",
		.clen = CLEN_UNKNOWN,
		.rd = {DTYP_FILE, GAND_RES_DATA(file) = "404.html"},
	};
}


/* server helpers */
static int
daemonise(void)
{
	int fd;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		/* i am the child */
		break;
	default:
		/* i am the parent */
		GAND_NOTI_LOG("Successfully bore a squaller: %d", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return -1;
	}
	for (int i = getdtablesize(); i>=0; --i) {
		/* close all descriptors */
		close(i);
	}
	if (LIKELY((fd = open("/dev/null", O_RDWR, 0)) >= 0)) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) {
			(void)close(fd);
		}
	}
	return 0;
}

static int
write_pidfile(const char *pidfile)
{
	char str[32];
	pid_t pid;
	ssize_t len;
	int fd;
	int res = 0;

	if (!(pid = getpid())) {
		res = -1;
	} else if ((len = snprintf(str, sizeof(str) - 1, "%d\n", pid)) < 0) {
		res = -1;
	} else if ((fd = open(pidfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		res = -1;
	} else {
		/* all's good */
		write(fd, str, len);
		close(fd);
	}
	return res;
}

static void
stat_cb(EV_P_ ev_stat *e, int UNUSED(revents))
{
	GAND_NOTI_LOG("symbol index file `%s' changed ...", e->path);
	if (gsymdb != NULL) {
		close_dict(gsymdb);
	}
	if ((gsymdb = open_dict(e->path, O_RDONLY)) == NULL) {
		GAND_ERR_LOG("cannot open symbol index file `%s': %s",
			     e->path, strerror(errno));
	} else {
		GAND_INFO_LOG(":inot symbol index file reloaded");
	}
	return;
}


#include "gandalfd.yucc"

int
main(int argc, char *argv[])
{
	/* args */
	static yuck_t argi[1U];
	gand_httpd_t h = NULL;
	int daemonisep = 0;
	short unsigned int port = 8080;
	/* paths and files */
	const char *pidf = NULL;
	const char *wwwd;
	static const char _trlf[] = "/var/scratch/freundt/trolf";
	static const char _dictf[] = DICT_DEFAULT;
	const char *dictf = NULL;
	/* inotify watcher */
	ev_stat dict_watcher;
	cfg_t cfg = NULL;
	int rc = 0;

	/* best not to be signalled for a minute */
	block_sigs();

	/* parse the command line */
	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto clos;
	}

	/* evaluate argi */
	daemonisep |= argi->daemon_flag;

	/* try and read the context file */
	if ((cfg = gand_read_config(argi->config_arg)) == NULL) {
		;
	} else {
		daemonisep |= cfg_glob_lookup_b(cfg, "daemonise");
	}

	/* run as daemon, do me properly */
	if (daemonisep) {
		int ret = daemonise();

		if (ret < 0) {
			perror("daemonisation failed");
			rc = 1;
			goto clos;
		} else if (ret > 0) {
			/* parent process */
			goto clos;
		}
	} else {
		/* fiddle with gandalf logging (default to syslog) */
		gand_log = gand_errlog;
	}

	/* start them log files */
	gand_openlog();

	/* write a pid file? */
	if ((pidf = argi->pidfile_arg) ||
	    (cfg && cfg_glob_lookup_s(&pidf, cfg, "pidfile") > 0)) {
		/* command line has precedence */
		if (write_pidfile(pidf) < 0) {
			GAND_ERR_LOG("cannot write pid file `%s': %s",
				     pidf, strerror(errno));
			rc = 1;
			goto clos;
		}
	}

	/* www dir? */
	if ((wwwd = argi->wwwdir_arg) ||
	    (cfg && cfg_glob_lookup_s(&wwwd, cfg, "wwwdir") > 0)) {
		/* command line has precedence */
		;
	}
	/* quick check here just before we go live */
	with (struct stat st) {
		if (wwwd == NULL) {
			GAND_ERR_LOG("wwwdir not specified");
			rc = 1;
			goto clos;
		} else if (stat(wwwd, &st) < 0) {
			GAND_ERR_LOG("cannot access wwwdir `%s': %s",
				     wwwd, strerror(errno));
			rc = 1;
			goto clos;
		} else if (!S_ISDIR(st.st_mode)) {
			GAND_ERR_LOG("wwwdir `%s' not a directory", wwwd);
			rc = 1;
			goto clos;
		} else if (access(wwwd, X_OK) < 0) {
			GAND_ERR_LOG("cannot access wwwdir `%s': %s",
				     wwwd, strerror(errno));
			rc = 1;
			goto clos;
		}
	}

	/* get the trolf dir */
	with (const char *trlf) {
		if ((trlf = argi->trolfdir_arg) ||
		    (cfg && cfg_glob_lookup_s(&trlf, cfg, "trolfdir") > 0)) {
			/* command line has precedence */
			;
		} else {
			/* preset with default */
			trlf = _trlf;
		}

		/* create trolf's dirfd */
		if ((trolf_dirfd = open(trlf, O_RDONLY)) < 0) {
			GAND_ERR_LOG("cannot access trolf directory: %s",
				     strerror(errno));
			rc = 1;
			goto clos;
		}

		/* try and find the dictionary */
		if ((dictf = argi->index_file_arg) ||
		    (cfg && cfg_glob_lookup_s(&dictf, cfg, "index_file") > 0)) {
			/* command line has precedence */
		;
		} else {
			/* preset with default */
			dictf = _dictf;
		}
		if ((gsymdb = open_dict(dictf, O_RDONLY)) == NULL) {
			size_t ntrlf = strlen(trlf);
			size_t ndict = strlen(dictf);
			char *tmpdf = malloc(ntrlf + 1U + ndict + 1U/*\nul*/);

			memcpy(tmpdf, trlf, ntrlf);
			if (tmpdf[ntrlf - 1] != '/') {
				tmpdf[ntrlf++] = '/';
			}
			memcpy(tmpdf + ntrlf, dictf, ndict);
			tmpdf[ntrlf + ndict] = '\0';

			/* final hand-over */
			dictf = tmpdf;
			if ((gsymdb = open_dict(dictf, O_RDONLY)) == NULL) {
				GAND_ERR_LOG("\
cannot open symbol index file `%s'", dictf);
				rc = 1;
				goto clos;
			}
		} else {
			/* just strdup dictf so we can access it all year round
			 * even when the cfg or the argi have been freed */
			dictf = strdup(dictf);
		}
	}

	/* server config */
	port = gand_get_port(cfg);
#define make_gand_httpd(p...)	make_gand_httpd((gand_httpd_param_t){p})
	/* configure the gand server */
	h = make_gand_httpd(
		.port = port, .timeout = 500000U,
		.www_dir = wwwd,
		.workf = work);
#undef make_gand_httpd

	if (UNLIKELY(h == NULL)) {
		GAND_ERR_LOG("cannot spawn gandalf server");
		rc = 1;
		goto clos;
	}

	/* we need an inotify on the dict file */
	with (void *loop = ev_default_loop(EVFLAG_AUTO)) {
		ev_stat_init(&dict_watcher, stat_cb, dictf, 0);
		ev_stat_start(EV_A_ &dict_watcher);
	}

	/* main loop */
	{
		/* set out sigs loose */
		unblock_sigs();
		/* and here we go */
		gand_httpd_run(h);
		/* not reached */
		block_sigs();
	}

	/* also we need an inotify on this guy */
	with (void *loop = ev_default_loop(EVFLAG_AUTO)) {
		ev_stat_stop(EV_A_ &dict_watcher);
	}

clos:
	/* away with the http */
	if (h != NULL) {
		free_gand_httpd(h);
	}

	/* kick the config context */
	if (cfg != NULL) {
		gand_free_config(cfg);
	}
	/* free cmdline parser goodness */
	yuck_free(argi);

	/* close trolf_dirfd */
	if (trolf_dirfd >= 0) {
		close(trolf_dirfd);
	}

	/* dictf was strdup'd */
	if (dictf != NULL) {
		free(deconst(dictf));
	}

	if (gsymdb != NULL) {
		close_dict(gsymdb);
	}
	if (pidf != NULL) {
		(void)unlink(pidf);
	}

	gand_closelog();
	return rc;
}

/* gandalfd.c ends here */

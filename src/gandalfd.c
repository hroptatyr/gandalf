/*** gandalfd.c -- rolf and milf accessor
 *
 * Copyright (C) 2011-2012 Sebastian Freundt
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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/time.h>
/* check for me */
#include <sys/sendfile.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */

#include "gandalf.h"
#include "logger.h"
#include "configger.h"
#include "ud-sock.h"
#include "gq.h"
#include "fileutils.h"
#include "slut.h"
#include "nifty.h"

/* we assume unserding with logger feature */
void *gand_logout;

#define GAND_MOD		"[mod/gand]"
#define GAND_INFO_LOG(args...)				\
	do {						\
		GAND_SYSLOG(LOG_INFO, GAND_MOD " " args);	\
		GAND_DEBUG("INFO " args);		\
	} while (0)
#define GAND_ERR_LOG(args...)					\
	do {							\
		GAND_SYSLOG(LOG_ERR, GAND_MOD " ERROR " args);	\
		GAND_DEBUG("ERROR " args);			\
	} while (0)
#define GAND_CRIT_LOG(args...)						\
	do {								\
		GAND_SYSLOG(LOG_CRIT, GAND_MOD " CRITICAL " args);	\
		GAND_DEBUG("CRITICAL " args);				\
	} while (0)
#define GAND_NOTI_LOG(args...)						\
	do {								\
		GAND_SYSLOG(LOG_NOTICE, GAND_MOD " NOTICE " args);	\
		GAND_DEBUG("NOTICE " args);				\
	} while (0)

/* sockaddr union */
typedef union ud_sockaddr_u *ud_sockaddr_t;
typedef const union ud_sockaddr_u *ud_const_sockaddr_t;

union ud_sockaddr_u {
	struct sockaddr_storage sas;
	struct sockaddr sa;
	struct sockaddr_in6 sa6;
};

/* the connection queue */
typedef struct ev_io_q_s *ev_io_q_t;
typedef struct ev_io_i_s *ev_io_i_t;

/* ev io object queue */
struct ev_io_q_s {
	struct gq_s q[1];
};

struct ev_io_i_s {
	struct gq_item_s i;
	ev_io w[1];
	uint16_t idx;
	/* reply buffer, pointer and size */
	char *rpl;
	size_t rsz;
};

/* match state */
struct ms_s {
	unsigned int cnt;
	/* the date that made a match */
	idate_t m_date;
	idate_t last;
};

/* match direction */
typedef enum {
	MDIR_NEVER = -1,
	MDIR_NOMATCH = 0,
	MDIR_MATCH,
} mdir_t;

#define MAX_MAP_TIME	(30U)


/* rolf glue */
static char *trolfdir;
static size_t ntrolfdir;

static char *nfo_fname = NULL;

static struct slut_s i2s_s[1];

static size_t
gand_get_trolfdir(char **tgt, cfg_t ctx)
{
	static char __trolfdir[] = "/var/scratch/freundt/trolf";
	size_t rsz;
	const char *res = NULL;
	cfgset_t *cs;

	if (UNLIKELY(ctx == NULL)) {
		goto dflt;
	}

	/* start out with an empty target */
	for (size_t i = 0, n = cfg_get_sets(&cs, ctx); i < n; i++) {
		if ((rsz = cfg_tbl_lookup_s(&res, ctx, cs[i], "trolfdir"))) {
			struct stat st = {0};

			if (stat(res, &st) == 0) {
				/* set up the IO watcher and timer */
				goto out;
			}
		}
	}

	/* otherwise try the root domain */
	if ((rsz = cfg_glob_lookup_s(&res, ctx, "trolfdir"))) {
		struct stat st = {0};

		if (stat(res, &st) == 0) {
			goto out;
		}
	}

	/* quite fruitless today */
dflt:
	res = __trolfdir;
	rsz = sizeof(__trolfdir) -1;

out:
	/* make sure *tgt is freeable */
	*tgt = strndup(res, rsz);
	return rsz;
}

static char*
gand_get_nfo_file(cfg_t ctx)
{
	static const char rinf[] = "rolft_info";
	static char f[PATH_MAX];
	cfgset_t *cs;
	size_t rsz;
	const char *res = NULL;
	size_t idx;

	if (UNLIKELY(ctx == NULL)) {
		goto dflt;
	}

	/* start out with an empty target */
	for (size_t i = 0, n = cfg_get_sets(&cs, ctx); i < n; i++) {
		if ((rsz = cfg_tbl_lookup_s(&res, ctx, cs[i], "nfo_file"))) {
			struct stat st = {0};

			if (stat(res, &st) == 0) {
				goto out;
			}
		}
	}

	/* otherwise try the root domain */
	if ((rsz = cfg_glob_lookup_s(&res, ctx, "nfo_file"))) {
		struct stat st = {0};

		if (stat(res, &st) == 0) {
			goto out;
		}
	}

	/* otherwise we'll construct it from the trolfdir */
dflt:
	if (UNLIKELY(trolfdir == NULL)) {
		return NULL;
	}

	/* construct the path */
	memcpy(f, trolfdir, (idx = ntrolfdir));
	if (f[idx - 1] != '/') {
		f[idx++] = '/';
	}
	memcpy(f + idx, rinf, sizeof(rinf) - 1);
	res = f;
	rsz = idx + sizeof(rinf) - 1;

out:
	/* make sure the return value is freeable */
	return strndup(res, rsz);
}

static uint16_t
gand_get_port(cfg_t ctx)
{
	cfgset_t *cs;
	int res;

	if (UNLIKELY(ctx == NULL)) {
		goto dflt;
	}

	/* start out with an empty target */
	for (size_t i = 0, n = cfg_get_sets(&cs, ctx); i < n; i++) {
		if ((res = cfg_tbl_lookup_i(ctx, cs[i], "port"))) {
			goto out;
		}
	}

	/* otherwise try the root domain */
	res = cfg_glob_lookup_i(ctx, "port");

out:
	if (res > 0 && res < 65536) {
		return (uint16_t)res;
	}
dflt:
	return 0U;
}

static size_t
mmmb_freeze(struct mmmb_s *mb)
{
	mb->buf = mremap(mb->buf, mb->all, mb->bsz, MREMAP_MAYMOVE);
	return mb->all = mb->bsz;
}

static const char*
make_lateglu_name(uint32_t rolf_id)
{
	static const char glud[] = "show_lateglu/";
	static char f[PATH_MAX];
	size_t idx;

	if (UNLIKELY(trolfdir == NULL)) {
		return NULL;
	}

	/* construct the path */
	memcpy(f, trolfdir, (idx = ntrolfdir));
	if (f[idx - 1] != '/') {
		f[idx++] = '/';
	}
	memcpy(f + idx, glud, sizeof(glud) - 1);
	idx += sizeof(glud) - 1;
	snprintf(
		f + idx, PATH_MAX - idx,
		/* this is the split version */
		"%04u/%08u", rolf_id / 10000U, rolf_id);
	return f;
}

static void
free_lateglu_name(const char *UNUSED(name))
{
	/* just for later when we're reentrant */
	return;
}

static mdir_t
match_date1_p(struct ms_s *state, const char *ln, size_t lsz, date_rng_t dr)
{
/* assume strict orderedness and fuck off early otherwise */
	const char *dt;
	idate_t idt;

	if ((dt = memchr(ln, '\t', lsz)) == NULL ||
	    (dt = memchr(dt + 1, '\t', lsz)) == NULL ||
	    (dt = memchr(dt + 1, '\t', lsz)) == NULL) {
		return MDIR_NOMATCH;
	}

	idt = __to_idate(dt + 1);
	if (dr->beg >= DATE_RNG_THEN && dr->end >= DATE_RNG_THEN) {
		if (idt >= dr->beg && idt <= dr->end) {
			return MDIR_MATCH;
		} else if (idt > dr->end) {
			return MDIR_NEVER;
		}
	} else {
		/* special thing to write 1992-03-03 -3 */
		if (dr->beg >= DATE_RNG_THEN && idt >= dr->beg) {
			if (idt == state->last) {
				/* trivially true */
				return MDIR_MATCH;
			} else if (state->cnt++ < dr->end) {
				state->last = idt;
				return MDIR_MATCH;
			}
			return MDIR_NOMATCH;
		}
	}
	return MDIR_NOMATCH;
}

static void
cancel_alts(struct valflav_s *vf, size_t idx)
{
	char *tmp = vf->this;

	/* swap this slot and the idx-th alternative */
	vf->this = vf->alts[idx];
	vf->alts[idx] = tmp;
	vf->nalts = 0;
	return;
}

static bool
match_valflav1_p(const char *ln, size_t lsz, struct valflav_s *vf)
{
	const char *a;
	const char *eoa;

	if ((a = memchr(ln, '\t', lsz)) == NULL ||
	    (a = memchr(a + 1, '\t', lsz)) == NULL ||
	    (a = memchr(a + 1, '\t', lsz)) == NULL ||
	    (a = memchr(a + 1, '\t', lsz)) == NULL ||
	    (eoa = memchr(++a, '\t', lsz)) == NULL) {
		/* no chance to match if we cant even find it */
		return false;
	}

	if (strncmp(vf->this, a, eoa - a) == 0) {
		/* never try any alternatives */
		vf->nalts = 0;
		return true;
	}
	for (size_t i = 0; i < vf->nalts; i++) {
		if (strncmp(vf->alts[i], a, eoa - a) == 0) {
			GAND_DEBUG(
				GAND_MOD " matched %zu-th alt, cancelling\n",
				i);
			cancel_alts(vf, i);
			return true;
		}
	}
	return false;
}

static mdir_t
match_msg_p(struct ms_s *state, const char *ln, size_t lsz, gand_msg_t msg)
{
/* special date syntax 2010-02-19 -3 means
 * 2010-02-19 and 2 points before that */
	mdir_t res;

	res = msg->ndate_rngs == 0 ? MDIR_MATCH : MDIR_NOMATCH;
	for (size_t i = 0; i < msg->ndate_rngs; i++) {
		date_rng_t mrng = msg->date_rngs + i;
		if ((res = match_date1_p(state, ln, lsz, mrng)) == MDIR_MATCH) {
			break;
		}
	}
	/* check */
	if (res < MDIR_MATCH) {
		return res;
	}

	res = msg->nvalflavs == 0 ? MDIR_MATCH : MDIR_NOMATCH;
	for (size_t i = 0; i < msg->nvalflavs; i++) {
		if (match_valflav1_p(ln, lsz, msg->valflavs + i)) {
			res = MDIR_MATCH;
			break;
		}
	}
	if (res < MDIR_MATCH) {
		return res;
	}
	return MDIR_MATCH;
}

#define BUF_INC			(4096)

static void
copy_between(struct mmmb_s *mb, const char *from, const char *to)
{
	memcpy(mb->buf + mb->bsz, from, to - from);
	mb->bsz += to - from;
	return;
}

static void
mmmbuf_check_resize(struct mmmb_s *mb, size_t lsz)
{
	if (mb->all == 0) {
		size_t ini_sz = (lsz & ~(BUF_INC - 1)) + BUF_INC;
		mb->buf = mmap(NULL, ini_sz, PROT_MEM, MAP_MEM, 0, 0);
		mb->all = ini_sz;
	} else if (mb->bsz + lsz + 1 > mb->all) {
		size_t new = ((mb->bsz + lsz) & ~(BUF_INC - 1)) + BUF_INC;
		mb->buf = mremap(mb->buf, mb->all, new, MREMAP_MAYMOVE);
		mb->all = new;
	}
	return;
}

static void
bang_line(struct mmmb_s *mb, const char *lin, size_t lsz, uint32_t sel)
{
	const char *tabs[5];

	/* check if we need to resize */
	mmmbuf_check_resize(mb, lsz);

	/* find all tabs first */
	{
		size_t ntab = 0U;

		for (const char *p = lin, *const ep = p + lsz; p < ep; p++) {
			if (*p == '\t') {
				tabs[ntab++] = p;
			}
		}

		if (UNLIKELY(ntab < countof(tabs))) {
			/* we need 5 tabs and found less, line is buggered */
			return;
		}
	}

	/* copy only interesting lines */
	if (sel & SEL_RID) {
		copy_between(mb, lin, tabs[0] + 1);
	}

	if (sel & SEL_SYM) {
		copy_between(mb, tabs[0] + 1, tabs[1] + 1);
	}

	if (sel & SEL_TID) {
		copy_between(mb, tabs[1] + 1, tabs[2] + 1);
	}

	if (sel & SEL_DATE) {
		copy_between(mb, tabs[2] + 1, tabs[3] + 1);
	}

	if (sel & SEL_VFLAV) {
		copy_between(mb, tabs[3] + 1, tabs[4] + 1);
	}

	if (sel & SEL_VALUE) {
		copy_between(mb, tabs[4] + 1, lin + lsz);
	}

	/* finalise the line */
	mb->buf[mb->bsz++] = '\n';
	return;
}

static void
bang_nfo_line(struct mmmb_s *mb, const char *lin, size_t lsz, uint32_t sel)
{
	const char *tabs[9];

	/* check if we need to resize */
	mmmbuf_check_resize(mb, lsz);

	/* find all tabs first */
	{
		size_t ntab = 0U;

		for (const char *p = lin, *const ep = p + lsz; p < ep; p++) {
			if (*p == '\t') {
				tabs[ntab++] = p;
			}
		}

		if (UNLIKELY(ntab < countof(tabs))) {
			/* we need 9 tabs and found less, line is buggered */
			return;
		}
	}

	/* copy only interesting lines */
	if (sel & SEL_RID) {
		copy_between(mb, lin, tabs[0] + 1);
	}

	if (sel & SEL_SYM) {
		copy_between(mb, tabs[1] + 1, tabs[2] + 1);
	}

	if (sel & SEL_DTRNG) {
		copy_between(mb, tabs[4] + 1, tabs[5] + 1);
	}

	if (sel & SEL_NPNT) {
		copy_between(mb, tabs[5] + 1, tabs[6] + 1);
	}

	if (sel & SEL_ALTSYM) {
		copy_between(mb, tabs[6] + 1, tabs[7] + 1);
	}

	if (sel & SEL_DISC) {
		copy_between(mb, tabs[7] + 1, tabs[8] + 1);
	}

	if (sel & SEL_DESCR) {
		copy_between(mb, tabs[8] + 1, lin + lsz);
	}

	/* finalise the line */
	mb->buf[mb->bsz++] = '\n';
	return;
}

static void
bang_whole_line(struct mmmb_s *mb, const char *lin, size_t lsz)
{
	/* check if we need to resize */
	mmmbuf_check_resize(mb, lsz);

	/* copy only interesting lines */
	memcpy(mb->buf + mb->bsz, lin, lsz);
	mb->bsz += lsz;

	/* finalise the line */
	mb->buf[mb->bsz++] = '\n';
	return;
}


static void
__get_ser(struct mmmb_s *mb, gand_msg_t msg, uint32_t rid)
{
	struct mmfb_s mf_ser = {.m = {NULL}, .fd = -1};
	struct ms_s state = {0};
	const char *f;

	GAND_INFO_LOG("get_ser(%u)\n", rid);
	/* get us the lateglu name */
	if ((f = make_lateglu_name(rid)) == NULL) {
		return;
	} else if (mmap_whole_file(&mf_ser, f) < 0) {
		goto out;
	}

	for (size_t idx = 0; idx < mf_ser.m.bsz; ) {
		const char *lin = mf_ser.m.buf + idx;
		char *eol = memchr(lin, '\n', mf_ser.m.bsz - idx);
		size_t lsz = eol - lin;
		int mdir;

#define DEFAULT_SEL	(SEL_SYM | SEL_DATE | SEL_VFLAV | SEL_VALUE)
		if ((mdir = match_msg_p(&state, lin, lsz, msg)) > 0) {
			bang_line(mb, lin, lsz, msg->sel ?: DEFAULT_SEL);
		} else if (mdir < 0) {
			/* there can't be any matches in this file */
			break;
		}
		idx += lsz + 1;
	}

	/* free the resources */
	munmap_all(&mf_ser);
out:
	free_lateglu_name(f);
	return;
}

static size_t
get_ser(char **buf, gand_msg_t msg)
{
	struct mmmb_s mb = {NULL};

	GAND_DEBUG("nrolf_objs %zu\n", msg->nrolf_objs);
	for (size_t i = 0; i < msg->nrolf_objs; i++) {
		rid_t rid;
		struct rolf_obj_s *robj = msg->rolf_objs + i;

		if (LIKELY(robj->rolf_id > 0)) {
			rid = robj->rolf_id;
		} else if (UNLIKELY(robj->rolf_sym == NULL)) {
			continue;
		} else if (UNLIKELY(msg->igncase == 1)) {
			const char *sym = robj->rolf_sym;
			rid = slut_isym2rid(i2s_s, sym);
		} else {
			const char *sym = robj->rolf_sym;
			rid = slut_sym2rid(i2s_s, sym);
		}
		GAND_DEBUG("rolf_obj %zu id %u\n", i, rid);
		if (LIKELY(rid != 0)) {
			__get_ser(&mb, msg, rid);
		}
	}

	/* prepare output */
	mmmb_freeze(&mb);
	*buf = mb.buf;
	return mb.bsz;
}

static struct nmfb_s
get_raw(gand_msg_t msg)
{
	struct nmfb_s res = {-1};
	const char *sym;
	const char *fn;
	rid_t rid;
	struct stat st = {0};

	GAND_DEBUG("nrolf_objs %zu\n", msg->nrolf_objs);
	if (UNLIKELY(msg->nrolf_objs != 1U)) {
		return res;
	} else if (UNLIKELY((sym = msg->rolf_objs[0].rolf_sym) == NULL)) {
		return res;
	} else if (UNLIKELY((rid = slut_sym2rid(i2s_s, sym)) == 0U)) {
		return res;
	} else if (UNLIKELY((fn = make_lateglu_name(rid)) == NULL)) {
		return res;
	} else if (UNLIKELY((res.fd = open(fn, O_RDONLY)) < 0)) {
		;
	} else if (UNLIKELY(fstat(res.fd, &st) < 0)) {
		;
	}

	res.fz = st.st_size;
	free_lateglu_name(fn);
	return res;
}

static struct mmfb_s mf_nfo = {
	.m = {
		.buf = NULL,
		.bsz = 0UL,
	},
	.fd = -1
};
static time_t mf_nfo_stamp;

static int
mmap_nfo(void)
{
	static struct timeval tv;

	if (LIKELY(mf_nfo.m.buf != NULL)) {
		/* still mapped, that's good */
		;
	} else if (nfo_fname == NULL) {
		return -1;
	} else if (mmap_whole_file(&mf_nfo, nfo_fname) < 0) {
		return -1;
	}
	/* keep track of last map time */
	(void)gettimeofday(&tv, NULL);
	mf_nfo_stamp = tv.tv_sec;
	return 0;
}

static int
munmap_nfo(int forcep)
{
	static struct timeval tv = {0};

	if (mf_nfo.m.buf == NULL) {
		/* do fuckall */
		return 0;
	} else if (((void)gettimeofday(&tv, NULL),
		    tv.tv_sec < mf_nfo_stamp + MAX_MAP_TIME) && !forcep) {
		/* don't munmap as the mapping is too recent */
		return 0;
	}
	/* otherwise it's definitely munmap time */
	GAND_NOTI_LOG("munmap'ping info file\n");
	munmap_all(&mf_nfo);
	return 0;
}

static void
__get_nfo(struct mmmb_s *mb, struct mmfb_s *mf, gand_msg_t msg, rid_t rid)
{
	struct slut_data_s rdata = slut_rid2data(i2s_s, rid);
	const char *cand = mf->m.buf + rdata.beg;
	const char *cend = mf->m.buf + rdata.end;

	GAND_INFO_LOG("get_nfo(%u)\n", rid);
	if (msg->sel == SEL_ALL || msg->sel == SEL_NOTHING) {
		bang_whole_line(mb, cand, cend - cand);
	} else {
		bang_nfo_line(mb, cand, cend - cand, msg->sel);
	}
	return;
}

static size_t
get_nfo(char **buf, gand_msg_t msg)
{
	struct mmmb_s mb = {NULL};

	/* general checks and get us the lateglu name */
	if (UNLIKELY(msg->nrolf_objs == 0)) {
		return 0UL;
	} else if (mmap_nfo() < 0) {
		return 0UL;
	}

	for (size_t i = 0; i < msg->nrolf_objs; i++) {
		struct rolf_obj_s *robj = msg->rolf_objs + i;
		rid_t rid;

		if (LIKELY(robj->rolf_id > 0)) {
			/* bugger, how to get rdata from rid? */
			rid = robj->rolf_id;
		} else if (UNLIKELY(robj->rolf_sym == NULL)) {
			continue;
		} else if (UNLIKELY(msg->igncase == 1)) {
			const char *sym = robj->rolf_sym;
			rid = slut_isym2rid(i2s_s, sym);
		} else {
			const char *sym = robj->rolf_sym;
			rid = slut_sym2rid(i2s_s, sym);
		}
		GAND_DEBUG("rolf_obj %zu id %u\n", i, rid);
		if (LIKELY(rid != 0)) {
			__get_nfo(&mb, &mf_nfo, msg, rid);
		}
	}

	/* prepare output */
	mmmb_freeze(&mb);
	*buf = mb.buf;
	return mb.bsz;
}


struct rsp_s {
	enum {
		RSP_UNK,
		RSP_BUF,
		RSP_NMP,
	} type;

	union {
		/* for RSP_BUF */
		struct {
			size_t z;
			char *buf;
		};

		/* for a descriptor based result, RSP_NMP */
		struct nmfb_s nmp;
	};
};

static struct rsp_s
interpret_msg(gand_msg_t msg)
{
	struct rsp_s res = {RSP_UNK};

	switch (gand_get_msg_type(msg)) {
	case GAND_MSG_GET_SER:
	case GAND_MSG_GET_DAT: {
		size_t len;
		char *buf;

		if ((len = get_ser(&buf, msg)) > 0U) {
			res.type = RSP_BUF;
			res.z = len;
			res.buf = buf;
		}
		break;
	}

	case GAND_MSG_GET_NFO: {
		size_t len;
		char *buf;

		if ((len = get_nfo(&buf, msg)) > 0U) {
			res.type = RSP_BUF;
			res.z = len;
			res.buf = buf;
		}
		break;
	}

	case GAND_MSG_GET_RAW:
		if ((res.nmp = get_raw(msg)).fd >= 0) {
			res.type = RSP_NMP;
		}
		break;

	default:
		GAND_ERR_LOG("unknown message %u\n", msg->hdr.mt);
		break;
	}
	return res;
}

static inline __attribute__((pure, const)) size_t
rsp_content_size(struct rsp_s x)
{
	switch (x.type) {
	case RSP_UNK:
	default:
		break;
	case RSP_BUF:
		return x.z;
	case RSP_NMP:
		return x.nmp.fz;
	}
	return 0U;
}

static ssize_t
send_http_wrap(int fd, size_t rsz, int flags)
{
	static char buf[] = "\
HTTP/1.1 xxx DESCRIPTION\r\n\
Date: bbb, dd aaa YYYY HH:MM:SS ZZZ\r\n\
Server: " PACKAGE_STRING "\n\
Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
Content-Type: text/plain; charset=utf-8\r\n\
Content-Length: 01234567\r\n\
\r\n";
	static const char r404[] = "404 Not Found  ";
	static const char r200[] = "200 OK         ";
	static const size_t bsz = sizeof(buf) - 1;
	/* offsets to stuff we overwrite */
	static const size_t status_off = sizeof("HTTP/1.1");
	static const size_t date_off = 32U;
	static const size_t clen_off = sizeof(buf) - 13U;

	if (LIKELY(rsz > 0)) {
		memcpy(buf + status_off, r200, sizeof(r200) - 1);
	} else {
		/* send 404 as is */
		memcpy(buf + status_off, r404, sizeof(r404) - 1);
	}
	/* everything else is the same in both cases (200 and 404) */
	{
		struct tm *tm;
		time_t now;

		time(&now);
		tm = gmtime(&now);
		strftime(buf + date_off, 30U, "%b, %d %a %Y %H:%M:%S UTC", tm);
		buf[date_off + 29] = '\r';
	}
	{
		snprintf(buf + clen_off, 9, "%8zu", rsz);
		buf[clen_off + 8] = '\r';
	}
	/* we will always send buf */
	return send(fd, buf, bsz, flags);
}

static int
handle_inot(const char *f)
{
	/* reinit */
	if (UNLIKELY(f == NULL)) {
		/* ah, we just wanted munmapping */
		GAND_INFO_LOG("sym table munmapped\n");
		return 0;
	} else if (slut_load(i2s_s, f) < 0) {
		GAND_ERR_LOG("sym table building failed\n");
		return 0;
	}
	/* not much that can go wrong now aye? */
	GAND_INFO_LOG("new sym table built (from %s)\n", f);
	munmap_nfo(1);
	/* never lose interest in these files */
	return 0;
}


/* connection handling */
#if !defined MAX_DCCP_CONNECTION_BACK_LOG
# define MAX_DCCP_CONNECTION_BACK_LOG	(5)
#endif	/* !MAX_DCCP_CONNECTION_BACK_LOG */

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
sock_listener(int s, ud_sockaddr_t sa)
{
	if (s < 0) {
		return s;
	}

	if (bind(s, &sa->sa, sizeof(*sa)) < 0) {
		return -1;
	}

	return listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
}

/* looks like dccp://host:port/secdef?idx=00000 */
static char brag_uri[INET6_ADDRSTRLEN + 64] = "dccp://";
/* offset into brag_uris idx= field */
static size_t UNUSED(brag_uri_offset) = 0;

static int
make_brag_uri(ud_sockaddr_t sa, socklen_t UNUSED(sa_len))
{
	struct utsname uts[1];
	char dnsdom[64];
	const size_t uri_host_offs = sizeof("dccp://");
	char *curs = brag_uri + uri_host_offs - 1;
	size_t rest = sizeof(brag_uri) - uri_host_offs;
	int len;

	if (uname(uts) < 0) {
		return -1;
	} else if (getdomainname(dnsdom, sizeof(dnsdom)) < 0) {
		return -1;
	}

	len = snprintf(
		curs, rest, "%s.%s:%hu/",
		uts->nodename, dnsdom, ntohs(sa->sa6.sin6_port));

	if (len > 0) {
		brag_uri_offset = uri_host_offs + len - 1;
	}
	return 0;
}

static inline short unsigned int
ud_sockaddr_fam(ud_const_sockaddr_t sa)
{
	return sa->sa.sa_family;
}

static inline short unsigned int
ud_sockaddr_port(ud_const_sockaddr_t sa)
{
	return ntohs(sa->sa6.sin6_port);
}

static inline const void*
ud_sockaddr_addr(ud_const_sockaddr_t sa)
{
	return &sa->sa6.sin6_addr;
}

static inline void
ud_sockaddr_ntop(char *restrict buf, size_t len, ud_const_sockaddr_t sa)
{
	short unsigned int fam = ud_sockaddr_fam(sa);
	const void *saa = ud_sockaddr_addr(sa);
	(void)inet_ntop(fam, saa, buf, len);
	return;
}


/* the actual beef, number of requests we can monitor */
#include "gq.c"

static struct ev_io_q_s ioq = {0};

static ev_io_i_t
make_io(void)
{
	ev_io_i_t res;

	if (ioq.q->free->i1st == NULL) {
		size_t nitems = ioq.q->nitems / sizeof(*res);

		assert(ioq.q->free->ilst == NULL);
		GAND_DEBUG("IOQ RESIZE -> %zu\n", nitems + 16);
		init_gq(ioq.q, sizeof(*res), nitems + 16);
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(ioq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static void
free_io(ev_io_i_t io)
{
	gq_push_tail(ioq.q->free, (gq_item_t)io);
	return;
}

/* fdfs */
static void
ev_io_shut(EV_P_ ev_io *w)
{
	int fd = w->fd;

	ev_io_stop(EV_A_ w);
	shutdown(fd, SHUT_RDWR);
	close(fd);
	w->fd = -1;
	return;
}

static void
ev_qio_shut(EV_P_ ev_io *w)
{
/* attention, W *must* come from the ev io queue */
	ev_io_i_t qio = w->data;

	ev_io_shut(EV_A_ w);
	free_io(qio);
	return;
}


/* callbacks */
static void
inot_cb(EV_P_ ev_stat *w, int UNUSED(re))
{
	GAND_INFO_LOG("INOT something changed in %s...\n", w->path);

	if (handle_inot(w->path) < 0) {
		goto clo;
	}
	return;
clo:
	ev_stat_stop(EV_A_ w);
	return;
}


static void
log_req(const char *buf, size_t bsz)
{
	const char *q = memchr(buf, '\n', bsz);
	static char cpy[256];

	if (LIKELY(q > buf && q < buf + sizeof(cpy) - 1)) {
		memcpy(cpy, buf, q - buf);
		cpy[q - buf] = '\0';
		GAND_INFO_LOG(":req [%s]\n", cpy);
	} else {
		GAND_INFO_LOG(":req <empty>\n");
	}
	return;
}

static void
dccp_data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	static char buf[4096];
	ssize_t nreq;
	struct rsp_s rsp;
	gand_msg_t msg;
	int keep_conn;

	if (UNLIKELY((nreq = read(w->fd, buf, sizeof(buf))) <= 4)) {
		goto clo;
	} else if (LIKELY((size_t)nreq < sizeof(buf))) {
		buf[nreq] = '\0';
	} else {
		/* uh oh, mega request, wtf? */
		buf[sizeof(buf) - 1] = '\0';
	}

	log_req(buf, nreq);
	if (UNLIKELY((msg = gand_parse_blob(NULL, buf, nreq)) == NULL)) {
		/* bugger right off */
		goto clo;
	}

	/* just do what they want, care about the format later */
	rsp = interpret_msg(msg);
	/* bring the response into shape */
	if (msg->hdr.flags & GAND_MSG_FLAG_WRAP_HTTP/*<-getter!!*/) {
		size_t z = rsp_content_size(rsp);

		send_http_wrap(w->fd, z, 0);
	}
	switch (rsp.type) {
	case RSP_UNK:
		break;
	case RSP_BUF:
		/* send off the result */
		send(w->fd, rsp.buf, rsp.z, 0);
		/* and clean up */
		munmap(rsp.buf, rsp.z);
		break;
	case RSP_NMP:
		sendfile(w->fd, rsp.nmp.fd, NULL, rsp.nmp.fz);
		break;
	}
	/* shouldn't this be a getter? */
	keep_conn = msg->hdr.flags & GAND_MSG_FLAG_KEEP_CONN;
	/* more clean up */
	gand_free_msg(msg);

	if (keep_conn) {
		return;
	}
clo:
	ev_qio_shut(EV_A_ w);
	return;
}

static void
log_conn(int fd, ud_const_sockaddr_t sa)
{
	static char abuf[INET6_ADDRSTRLEN];
	short unsigned int p;

	ud_sockaddr_ntop(abuf, sizeof(abuf), sa);
	p = ud_sockaddr_port(sa);
	GAND_INFO_LOG(":sock %d connect :from [%s]:%d\n", fd, abuf, p);
	return;
}


static void
dccp_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	union ud_sockaddr_u sa;
	socklen_t sasz = sizeof(sa);
	ev_io_i_t qio;
	int s;

	if ((s = accept(w->fd, &sa.sa, &sasz)) < 0) {
		return;
	}
	log_conn(s, &sa);

	qio = make_io();
	ev_io_init(qio->w, dccp_data_cb, s, EV_READ);
	qio->w->data = qio;
	ev_io_start(EV_A_ qio->w);
	return;
}

static void
prep_cb(EV_P_ ev_prepare *UNUSED(w), int UNUSED(revents))
{
	/* check the map stamp */
	munmap_nfo(0);
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	GAND_NOTI_LOG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	GAND_NOTI_LOG("SIGPIPE caught, doing nothing\n");
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	GAND_NOTI_LOG("SIGHUP caught, doing nothing\n");
	return;
}


#define GLOB_CFG_PRE	"/etc/unserding"
#if !defined MAX_PATH_LEN
# define MAX_PATH_LEN	64
#endif	/* !MAX_PATH_LEN */

/* do me properly */
static const char cfg_glob_prefix[] = GLOB_CFG_PRE;

#if defined USE_LUA
/* that should be pretty much the only mention of lua in here */
static const char cfg_file_name[] = "gandalf.lua";
#endif	/* USE_LUA */

static void
gand_expand_user_cfg_file_name(char *tgt)
{
	char *p;
	const char *homedir = getenv("HOME");
	size_t homedirlen = strlen(homedir);

	/* get the user's home dir */
	memcpy(tgt, homedir, homedirlen);
	p = tgt + homedirlen;
	*p++ = '/';
	*p++ = '.';
	strncpy(p, cfg_file_name, sizeof(cfg_file_name));
	return;
}

static void
gand_expand_glob_cfg_file_name(char *tgt)
{
	char *p;

	/* get the user's home dir */
	strncpy(tgt, cfg_glob_prefix, sizeof(cfg_glob_prefix));
	p = tgt + sizeof(cfg_glob_prefix);
	*p++ = '/';
	strncpy(p, cfg_file_name, sizeof(cfg_file_name));
	return;
}

static cfg_t
gand_read_config(const char *user_cf)
{
	char cfgf[MAX_PATH_LEN];
	cfg_t cfg;

        GAND_DEBUG("reading configuration from config file ...");

	/* we prefer the user's config file, then fall back to the
	 * global config file if that's not available */
	if (user_cf != NULL && (cfg = configger_init(user_cf)) != NULL) {
		GAND_DBGCONT("done\n");
		return cfg;
	}

	gand_expand_user_cfg_file_name(cfgf);
	if (cfgf != NULL && (cfg = configger_init(cfgf)) != NULL) {
		GAND_DBGCONT("done\n");
		return cfg;
	}

	/* otherwise there must have been an error */
	gand_expand_glob_cfg_file_name(cfgf);
	if (cfgf != NULL && (cfg = configger_init(cfgf)) != NULL) {
		GAND_DBGCONT("done\n");
		return cfg;
	}
	GAND_DBGCONT("failed\n");
	return NULL;
}

static void
gand_free_config(cfg_t ctx)
{
	if (ctx != NULL) {
		configger_fini(ctx);
	}
	return;
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
		break;
	default:
		GAND_NOTI_LOG("Successfully bore a squaller: %d\n", pid);
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
	gand_logout = fopen("/dev/null", "w");
	return 0;
}

static void
write_pidfile(const char *pidfile)
{
	char str[32];
	pid_t pid;
	size_t len;
	int fd;

	if ((pid = getpid()) &&
	    (len = snprintf(str, sizeof(str) - 1, "%d\n", pid)) &&
	    (fd = open(pidfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) >= 0) {
		write(fd, str, len);
		close(fd);
	} else {
		GAND_ERR_LOG("Could not write pid file %s\n", pidfile);
	}
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "gandalfd-clo.h"
#include "gandalfd-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	static ev_signal ALGN16(sigint_watcher)[1];
	static ev_signal ALGN16(sighup_watcher)[1];
	static ev_signal ALGN16(sigterm_watcher)[1];
	static ev_signal ALGN16(sigpipe_watcher)[1];
	/* our communication sockets */
	ev_io lstn[2];
	size_t nlstn = 0;
	/* i2s translation */
	ev_stat ALGN16(st_i2s)[1];
	/* prep timer */
	ev_prepare prp[1];
	/* args */
	struct gengetopt_args_info argi[1];
	/* our take on args */
	int daemonisep = 0;
	uint16_t port;
	cfg_t cfg;

	/* whither to log */
	gand_logout = stderr;

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
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
	if (daemonisep && daemonise() < 0) {
		perror("daemonisation failed");
		exit(1);
	}

	/* start them log files */
	gand_openlog();

	/* write a pid file? */
	{
		const char *pidf;

		if ((argi->pidfile_given && (pidf = argi->pidfile_arg)) ||
		    (cfg && cfg_glob_lookup_s(&pidf, cfg, "pidfile") > 0)) {
			/* command line has precedence */
			write_pidfile(pidf);
		}
	}

	/* get the trolf dir */
	ntrolfdir = gand_get_trolfdir(&trolfdir, cfg);
	nfo_fname = gand_get_nfo_file(cfg);
	port = gand_get_port(cfg);

	/* free cmdline parser goodness */
	cmdline_parser_free(argi);
	/* kick the config context */
	gand_free_config(cfg);

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* pre and post poll hooks */
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sigint_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* get us this socket thing */
	{
		union ud_sockaddr_u sa = {
			.sa6 = {
				.sin6_family = AF_INET6,
				.sin6_addr = in6addr_any,
				.sin6_port = htons(port),
			},
		};
		socklen_t sa_len = sizeof(sa);
		int s;

		if ((s = make_tcp()) < 0) {
			/* just to indicate we have no socket */
			lstn[0].fd = -1;
		} else if (sock_listener(s, &sa) < 0) {
			/* bugger */
			close(s);
			lstn[0].fd = -1;
		} else {
			/* yay */
			ev_io_init(lstn + 0, dccp_cb, s, EV_READ);
			ev_io_start(EV_A_ lstn);

			getsockname(s, &sa.sa, &sa_len);
			nlstn++;

			/* brag about this url */
			make_brag_uri(&sa, sa_len);
			GAND_INFO_LOG("%s\n", brag_uri);
		}
	}

	/* inotify the symbol file */
	if (nfo_fname != NULL) {
		ev_stat_init(st_i2s, inot_cb, nfo_fname, 0.);
		ev_stat_start(EV_A_ st_i2s);

		/* and just to make sure we kick things off */
		GAND_INFO_LOG("building symtable, stand by please ...\n");
		make_slut(i2s_s);
		handle_inot(nfo_fname);
	}

	GAND_NOTI_LOG("gandalfd ready\n");

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	GAND_NOTI_LOG("shutting down gandalfd\n");

	/* munmap the info file (if mapped) */
	munmap_nfo(/*force*/1);

	/* stop inotifying */
	handle_inot(NULL);
	ev_stat_stop(EV_A_ st_i2s);
	free_slut(i2s_s);

	/* stop them post poll hooks */
	ev_prepare_stop(EV_A_ prp);

	/* close the listener */
	for (size_t i = 0; i < nlstn; i++) {
		int s = lstn[i].fd;
		ev_io_stop(EV_A_ lstn + i);
		close(s);
	}

	/* destroy the default evloop */
	ev_default_destroy();

	/* free trolfdir and nfo_fname */
	if (LIKELY(trolfdir != NULL)) {
		free(trolfdir);
	}
	if (LIKELY(nfo_fname != NULL)) {
		free(nfo_fname);
	}

	/* close our log output */	
	fflush(gand_logout);
	fclose(gand_logout);
	gand_closelog();
	/* unloop was called, so exit */
	return 0;
}

/* gandalfd.c ends here */

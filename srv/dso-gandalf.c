/*** dso-gandalf.c -- rolf and milf, aka time series and instruments
 *
 * Copyright (C) 2011 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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
#include <sys/mman.h>
#include <sys/stat.h>

#include <unserding/unserding-ctx.h>
#include <unserding/unserding-cfg.h>
#include <unserding/module.h>
#include <unserding/tcp-unix.h>

#include "gandalf.h"
#include "nifty.h"

/* we assume unserding with logger feature */
#define GAND_MOD		"[mod/gand]"
#define GAND_INFO_LOG(args...)	UD_SYSLOG(LOG_INFO, GAND_MOD " " args)
#define GAND_ERR_LOG(args...)	UD_SYSLOG(LOG_ERR, GAND_MOD " ERROR " args)
#define GAND_CRIT_LOG(args...)	UD_SYSLOG(LOG_CRIT, GAND_MOD " CRITICAL " args)

/* mmap buffers, memory and file based */
struct mmmb_s {
	char *buf;
	/* real size */
	size_t bsz;
	/* alloc size */
	size_t all;
};

struct mmfb_s {
	struct mmmb_s m;
	/* file desc */
	int fd;
};

static char *trolfdir;
static size_t ntrolfdir;
/* rolf symbol file */
static struct mmfb_s grsym = {
	.m = {
		.buf = NULL,
		.bsz = 0UL,
		.all = 0UL,
	},
	.fd = -1,
};

static struct mmfb_s grsymu = {
	.m = {
		.buf = NULL,
		.bsz = 0UL,
		.all = 0UL,
	},
	.fd = -1,
};

/* match state */
struct ms_s {
	int cnt;
	/* the date that made a match */
	idate_t m_date;
	idate_t last;
};


/* connexion<->proto glue */
static int
wr_fin_cb(ud_conn_t UNUSED(c), char *buf, size_t bsz, void *UNUSED(data))
{
	munmap(buf, bsz);
	return 0;
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
	snprintf(f + idx, PATH_MAX - idx, "%08u", rolf_id);
	return f;
}

static void
free_lateglu_name(const char *UNUSED(name))
{
	/* just for later when we're reentrant */
	return;
}

static const char*
make_trolf_name(const char *post, size_t plen)
{
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
	memcpy(f + idx, post, plen);
	return f;
}

static const char*
make_info_name(void)
{
	static const char rinf[] = "rolft_info";
	static char f[PATH_MAX];
	static bool inip = false;
	size_t idx;

	if (LIKELY(inip)) {
	singleton:
		return f;
	} else if (UNLIKELY(trolfdir == NULL)) {
		return NULL;
	}

	/* construct the path */
	memcpy(f, trolfdir, (idx = ntrolfdir));
	if (f[idx - 1] != '/') {
		f[idx++] = '/';
	}
	memcpy(f + idx, rinf, sizeof(rinf) - 1);
	inip = true;
	goto singleton;
}

static void
free_info_name(const char *UNUSED(sym))
{
	return;
}


static int
mmap_whole_file(struct mmfb_s *mf, const char *f, size_t fsz)
{
	if (LIKELY(fsz == 0)) {
		struct stat st[1];
		if (UNLIKELY(stat(f, st) < 0 || (fsz = st->st_size) == 0)) {
			return -1;
		}
	}
	if (UNLIKELY((mf->fd = open(f, O_RDONLY)) < 0)) {
		return -1;
	}

	/* mmap the file */
	mf->m.buf = mmap(NULL, fsz, PROT_READ, MAP_SHARED, mf->fd, 0);
	return mf->m.all = mf->m.bsz = fsz;
}

static void
munmap_all(struct mmfb_s *mf)
{
	if (mf->m.buf != NULL && mf->m.all > 0UL) {
		munmap(mf->m.buf, mf->m.all);
	}
	if (mf->fd >= 0) {
		close(mf->fd);
	}
	/* reset values */
	mf->m.buf = NULL;
	mf->m.bsz = mf->m.all = 0UL;
	mf->fd = -1;
	return;
}

static bool
match_date1_p(struct ms_s *state, const char *ln, size_t lsz, date_rng_t dr)
{
	const char *dt;
	idate_t idt;

	if ((dt = memchr(ln, '\t', lsz)) == NULL ||
	    (dt = memchr(dt + 1, '\t', lsz)) == NULL ||
	    (dt = memchr(dt + 1, '\t', lsz)) == NULL) {
		return false;
	}

	idt = __to_idate(dt + 1);
	if (dr->beg >= DATE_RNG_THEN && dr->end >= DATE_RNG_THEN) {
		if (idt >= dr->beg && idt <= dr->end) {
			return true;
		}
	} else {
		/* special thing to write 1992-03-03 -3 */
		if (dr->beg >= DATE_RNG_THEN && idt >= dr->beg) {
			if (idt == state->last) {
				/* trivially true */
				return true;
			} else if (state->cnt++ < dr->end) {
				state->last = idt;
				return true;
			}
			return false;
		}
	}
	return false;
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

static bool
match_msg_p(struct ms_s *state, const char *ln, size_t lsz, gand_msg_t msg)
{
/* special date syntax 2010-02-19 -3 means
 * 2010-02-19 and 2 points before that */
	bool res;

	res = msg->ndate_rngs == 0;
	for (size_t i = 0; i < msg->ndate_rngs; i++) {
		if (match_date1_p(state, ln, lsz, msg->date_rngs + i)) {
			res = true;
			break;
		}
	}
	/* check */
	if (!res) {
		return false;
	}

	res = msg->nvalflavs == 0;
	for (size_t i = 0; i < msg->nvalflavs; i++) {
		if (match_valflav1_p(ln, lsz, msg->valflavs + i)) {
			res = true;
			break;
		}
	}
	if (!res) {
		return false;
	}
	return true;
}

#define PROT_MEM		(PROT_READ | PROT_WRITE)
#define MAP_MEM			(MAP_PRIVATE | MAP_ANONYMOUS)
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
	const char *tabs[6];

	/* check if we need to resize */
	mmmbuf_check_resize(mb, lsz);

	/* find all tabs first */
	tabs[0] = rawmemchr(lin, '\t');
	tabs[1] = rawmemchr(tabs[0] + 1, '\t');
	tabs[2] = rawmemchr(tabs[1] + 1, '\t');
	tabs[3] = rawmemchr(tabs[2] + 1, '\t');
	tabs[4] = rawmemchr(tabs[3] + 1, '\t');
	tabs[5] = rawmemchr(tabs[4] + 1, '\t');

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

	if (sel & SEL_VFID) {
		copy_between(mb, tabs[3] + 1, tabs[4] + 1);
	}

	if (sel & SEL_VFLAV) {
		copy_between(mb, tabs[4] + 1, tabs[5] + 1);
	}

	if (sel & SEL_VALUE) {
		copy_between(mb, tabs[5] + 1, lin + lsz);
	}

	/* finalise the line */
	mb->buf[mb->bsz - 1] = '\n';
	return;
}

static void
bang_nfo_line(struct mmmb_s *mb, const char *lin, size_t lsz, uint32_t sel)
{
	const char *tabs[9];

	/* check if we need to resize */
	mmmbuf_check_resize(mb, lsz);

	/* find all tabs first */
	tabs[0] = rawmemchr(lin, '\t');
	tabs[1] = rawmemchr(tabs[0] + 1, '\t');
	tabs[2] = rawmemchr(tabs[1] + 1, '\t');
	tabs[3] = rawmemchr(tabs[2] + 1, '\t');
	tabs[4] = rawmemchr(tabs[3] + 1, '\t');
	tabs[5] = rawmemchr(tabs[4] + 1, '\t');
	tabs[6] = rawmemchr(tabs[5] + 1, '\t');
	tabs[7] = rawmemchr(tabs[6] + 1, '\t');
	tabs[8] = rawmemchr(tabs[7] + 1, '\t');

	/* copy only interesting lines */
	if (sel & SEL_RID) {
		copy_between(mb, lin, tabs[0] + 1);
	}

	if (sel & SEL_QID) {
		copy_between(mb, tabs[0] + 1, tabs[1] + 1);
	}

	if (sel & SEL_SYM) {
		copy_between(mb, tabs[1] + 1, tabs[2] + 1);
	}

	if (sel & SEL_DATE) {
		copy_between(mb, tabs[4] + 1, tabs[5] + 1);
	}

	if (sel & SEL_VALUE) {
		copy_between(mb, tabs[5] + 1, tabs[6] + 1);
	}

	if (sel & SEL_VFID) {
		copy_between(mb, tabs[6] + 1, tabs[7] + 1);
	}

	if (sel & SEL_DISC) {
		copy_between(mb, tabs[7] + 1, tabs[8] + 1);
	}

	/* finalise the line */
	mb->buf[mb->bsz - 1] = '\n';
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
	struct mmfb_s mf = {.m = {0}, .fd = -1};
	struct ms_s state = {0};
	const char *f;

	GAND_INFO_LOG("get_ser(%u)", rid);
	/* get us the lateglu name */
	if ((f = make_lateglu_name(rid)) == NULL) {
		return;
	} else if (mmap_whole_file(&mf, f, 0) < 0) {
		goto out;
	}

	for (size_t idx = 0; idx < mf.m.bsz; ) {
		const char *lin = mf.m.buf + idx;
		char *eol = memchr(lin, '\n', mf.m.bsz - idx);
		size_t lsz = eol - lin;

#define DEFAULT_SEL	(SEL_SYM | SEL_DATE | SEL_VFLAV | SEL_VALUE)
		if (match_msg_p(&state, lin, lsz, msg)) {
			bang_line(mb, lin, lsz, msg->sel ?: DEFAULT_SEL);
		}
		idx += lsz + 1;
	}

	/* free the resources */
	munmap_all(&mf);
out:
	free_lateglu_name(f);
	return;
}


struct get_rid_iter_s {
	const struct mmfb_s *symbuf;
	const char *bufpos;
	const char *sym;
	size_t ssz;
};

static inline struct get_rid_iter_s
init_rid_iter(const struct mmfb_s *symbuf, const char *sym, size_t len)
{
	struct get_rid_iter_s res = {
		.symbuf = symbuf,
		.bufpos = NULL,
		.sym = sym,
		.ssz = len,
	};
	return res;
}

static inline void
fini_rid_iter(struct get_rid_iter_s *s)
{
	s->symbuf = NULL;
	s->bufpos = NULL;
	s->sym = NULL;
	s->ssz = 0;
	return;
}

static inline bool
rid_iter_next_p(struct get_rid_iter_s *s)
{
	return s->symbuf != NULL && s->ssz == 0;
}

static uint32_t
rid_iter(struct get_rid_iter_s *st)
{
	const struct mmfb_s *symbuf = st->symbuf;
	const char *sym = st->sym;
	size_t ssz = st->ssz;
	const char *cand;
	ssize_t rest;

	/* set up */
	if (UNLIKELY(st->bufpos != NULL)) {
		cand = st->bufpos;
		rest = symbuf->m.bsz - (st->bufpos - symbuf->m.buf);
	} else {
		cand = symbuf->m.buf;
		rest = symbuf->m.bsz;
	}
	while ((cand = memmem(cand, rest, sym, ssz))) {
		if (cand == symbuf->m.buf || cand[-1] == '\n') {
			/* we've got a prefix match */
			uint32_t rid;
			char *next = NULL;

			rest = symbuf->m.bsz - (cand - symbuf->m.buf);
			cand = memchr(cand, '\t', rest);
			rid = strtoul(cand + 1, &next, 10);
			st->bufpos = next ?: cand + 1;
			return rid;
		}
		if ((rest = symbuf->m.bsz - (++cand - symbuf->m.buf)) <= 0) {
			break;
		}
	}
	fini_rid_iter(st);
	return 0U;
}

static size_t
__upcase(char *str)
{
	size_t len;

	for (len = 0; *str; len++, str++) {
		switch (*str) {
		case 'a' ... 'z':
			*str -= 0x20;
		default:
			break;
		}
	}
	return len;
}

static size_t
get_ser(char **buf, gand_msg_t msg)
{
	struct mmmb_s mb = {0};

	GAND_DEBUG("nrolf_objs %zu\n", msg->nrolf_objs);
	for (size_t i = 0; i < msg->nrolf_objs; i++) {
		uint32_t rid;
		struct rolf_obj_s *robj = msg->rolf_objs + i;
		struct get_rid_iter_s st;

		if (UNLIKELY(robj->rolf_sym == NULL)) {
			continue;
		} else if (LIKELY(robj->rolf_id > 0)) {
			rid = robj->rolf_id;
			fini_rid_iter(&st);
			goto proc;
		} else if (UNLIKELY(msg->igncase == 1)) {
			char *sym = robj->rolf_sym;
			size_t len = __upcase(sym);
			st = init_rid_iter(&grsymu, sym, len);
		} else {
			const char *sym = robj->rolf_sym;
			size_t len = strlen(sym);
			st = init_rid_iter(&grsym, sym, len);
		}
		do {
			rid = rid_iter(&st);
		proc:
			GAND_DEBUG("rolf_obj %zu id %u\n", i, rid);
			__get_ser(&mb, msg, rid);
		} while (rid_iter_next_p(&st));
	}

	/* prepare output */
	mmmb_freeze(&mb);
	*buf = mb.buf;
	return mb.bsz;
}

static const char*
__bol(const char *ptr, size_t bsz)
{
	const char *tmp;
	const char *bop = ptr - bsz;

	if (UNLIKELY((tmp = memrchr(bop, '\n', bsz)) == NULL)) {
		return bop;
	}
	return tmp + 1;
}

static const char*
__eol(const char *ptr, size_t bsz)
{
	const char *tmp = memchr(ptr, '\n', bsz);

	if (UNLIKELY(tmp == NULL)) {
		return tmp + bsz;
	}
	return tmp;
}

static size_t
get_nfo(char **buf, gand_msg_t msg)
{
	struct mmmb_s mb = {0};
	struct mmfb_s mf = {.m = {0}, .fd = -1};
	const char *f;

	/* general checks and get us the lateglu name */
	if (UNLIKELY(msg->nrolf_objs == 0)) {
		return 0UL;
	} else if ((f = make_info_name()) == NULL) {
		return 0UL;
	} else if (mmap_whole_file(&mf, f, 0) < 0) {
		goto out;
	}

	for (size_t i = 0; i < msg->nrolf_objs; i++) {
		char rids[8];
		struct rolf_obj_s *ro = msg->rolf_objs + i;
		const char *p;
		size_t q;
		size_t rest;

		if (LIKELY(ro->rolf_id > 0)) {
			q = snprintf(rids, sizeof(rids), "%u", ro->rolf_id);
			p = rids;
		} else {
			p = ro->rolf_sym;
			q = strlen(ro->rolf_sym);
		}

		for (const char *cand = (rest = mf.m.bsz, mf.m.buf), *cend;
		     (cand = memmem(cand, rest, p, q)) != NULL;
		     cand++, rest = mf.m.bsz - (cand - mf.m.buf)) {
			if (ro->rolf_id > 0) {
				/* rolf ids are only at the
				 * beginning of a line */
				if (!((cand == mf.m.buf || cand[-1] == '\n') &&
				      (cand[q] == '\t'))) {
					continue;
				}
			} else if (!(cand[-1] == '\t' &&
				     rawmemchr(cand, '@') <
				     rawmemchr(cand, '\t'))) {
				continue;
			}

			/* search for bol/eol */
			cand = __bol(cand, cand - mf.m.buf);
			cend = __eol(cand, mf.m.bsz - (cand - mf.m.buf));

			/* bang the line */
			if (msg->sel == SEL_ALL || msg->sel == SEL_NOTHING) {
				bang_whole_line(&mb, cand, cend - cand);
			} else {
				bang_nfo_line(&mb, cand, cend - cand, msg->sel);
			}
			cand = cend;
		}
	}

	/* prepare output */
	mmmb_freeze(&mb);
	*buf = mb.buf;
	/* free the resources */
	munmap_all(&mf);
out:
	free_info_name(f);
	return mb.bsz;
}

static size_t
interpret_msg(char **buf, gand_msg_t msg)
{
	size_t len = 0;

	switch (gand_get_msg_type(msg)) {
	case GAND_MSG_GET_SER:
		len = get_ser(buf, msg);
		break;

	case GAND_MSG_GET_DAT:
		len = get_ser(buf, msg);
		break;

	case GAND_MSG_GET_NFO:
		len = get_nfo(buf, msg);
		break;

	default:
		GAND_ERR_LOG("unknown message %u", msg->hdr.mt);
		break;
	}
	/* free 'im 'ere */
	gand_free_msg(msg);
	return len;
}

/**
 * Take the stuff in MSG of size MSGLEN coming from FD and process it.
 * Return values <0 cause the handler caller to close down the socket. */
static int
handle_data(ud_conn_t c, char *msg, size_t msglen, void *data)
{
	gand_ctx_t p = data;
	gand_msg_t umsg;

	GAND_DEBUG(GAND_MOD "/ctx: %p %zu\n", c, msglen);
#if defined DEBUG_FLAG
	/* safely write msg to logerr now */
	fwrite(msg, msglen, 1, gand_logout);
#endif	/* DEBUG_FLAG */
	GAND_INFO_LOG("%s", msg);

	/* just to avoid confusion */
	if ((umsg = gand_parse_blob_r(&p, msg, msglen)) != NULL) {
		/* definite success */
		char *buf = NULL;
		size_t len;
		ud_conn_t wr = NULL;

		/* serialise, put results in BUF*/
		if ((len = interpret_msg(&buf, umsg)) &&
		    (wr = ud_write_soon(c, buf, len, wr_fin_cb))) {
			ud_conn_put_data(wr, buf);
			return 0;
		} else if (buf && len) {
			wr_fin_cb(NULL, buf, len, NULL);
		}
		p = NULL;

	} else if (/* umsg == NULL && */p == NULL) {
		/* error occurred */
		GAND_ERR_LOG("context has disappeared, cleaning up");
	} else {
		GAND_INFO_LOG("need more grub");
	}
	ud_conn_put_data(c, p);
	return 0;
}

static int
handle_close(ud_conn_t c, void *data)
{
	if (data) {
		/* finalise the push parser to avoid mem leaks */
		gand_msg_t msg = gand_parse_blob_r(&data, data, 0);

		if (UNLIKELY(msg != NULL)) {
			/* sigh */
			gand_free_msg(msg);
		}
	}
	ud_conn_put_data(c, NULL);
	return 0;
}

static int
handle_inot(
	ud_conn_t UNUSED(c), const char *f, const struct stat *st, void *data)
{
	/* check if someone trunc'd us the file */
	if (UNLIKELY(st == NULL)) {
		/* good */
	} else if (UNLIKELY(st->st_size == 0)) {
		return -1;
	}

	/* off with the old guy */
	munmap_all(data);
	/* reinit */
	GAND_INFO_LOG("building sym table \"%s\" ...", f);
	if (mmap_whole_file(data, f, st ? st->st_size : 0) < 0) {
		GAND_ERR_LOG("sym table building failed");
		return -1;
	}
	GAND_INFO_LOG("new sym table built");
	return 0;
}


static ud_conn_t
gand_init_inot(const char *file, struct mmfb_s *symbuf)
{
	ud_conn_t res = make_inot_conn(file, handle_inot, symbuf);
	/* god i'm a hacker */
	handle_inot(res, file, NULL, symbuf);
	return res;
}

static ud_conn_t
gand_init_uds_sock(const char **sock_path, ud_ctx_t ctx, void *settings)
{
	volatile int res = -1;

	udcfg_tbl_lookup_s(sock_path, ctx, settings, "sock");
	return make_unix_conn(*sock_path, handle_data, handle_close, NULL);
}

static ud_conn_t
gand_init_net_sock(ud_ctx_t ctx, void *settings)
{
	int port;

	port = udcfg_tbl_lookup_i(ctx, settings, "port");
	return make_tcp_conn(port, handle_data, handle_close, NULL);
}

static size_t
gand_get_trolfdir(char **tgt, ud_ctx_t ctx, void *settings)
{
	size_t rsz;
	const char *res;

	*tgt = NULL;
	if ((rsz = udcfg_tbl_lookup_s(&res, ctx, settings, "trolfdir"))) {
		struct stat st[1];

		if (stat(res, st) == 0) {
			/* set up the IO watcher and timer */
			*tgt = strndup(res, rsz);
		}
	}
	return rsz;
}


/* unserding bindings */
static ud_conn_t __cnet = NULL;
static ud_conn_t __cuds = NULL;
static ud_conn_t __sym_inot = NULL;
static ud_conn_t __symu_inot = NULL;
/* path to unix domain socket */
static const char *gand_sock_path;
void *gand_logout;

void
init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	GAND_INFO_LOG("loading gandalf module");

	/* glue to lua settings */
	if ((settings = udctx_get_setting(ctx)) == NULL) {
		GAND_ERR_LOG("settings could not be read\n");
		return;
	}
	/* obtain the unix domain sock from our settings */
	__cuds = gand_init_uds_sock(&gand_sock_path, ctx, settings);
	/* obtain port number for our network socket */
	__cnet = gand_init_net_sock(ctx, settings);
	ntrolfdir = gand_get_trolfdir(&trolfdir, ctx, settings);
	/* inotify the symbol file */
	{
		static const char symn[] = "rolft_symbol";
		static const char symun[] = "rolft_symbol_u";
		const char *tmp;

		tmp = make_trolf_name(symn, sizeof(symn) - 1);
		__sym_inot = gand_init_inot(tmp, &grsym);

		tmp = make_trolf_name(symun, sizeof(symun) - 1);
		__symu_inot = gand_init_inot(tmp, &grsymu);
	}

	GAND_INFO_LOG("successfully loaded, trolfdir is %s", trolfdir);

#if defined DEBUG_FLAG
	gand_logout = stderr;
#else  /* !DEBUG_FLAG */
	gand_logout = fopen("/dev/null", "w");
#endif	/* DEBUG_FLAG */

	/* clean up */
	udctx_set_setting(ctx, NULL);
	return;
}

void
reinit(void *UNUSED(clo))
{
	GAND_INFO_LOG("reloading gandalf module ... done");
	return;
}

void
deinit(void *UNUSED(clo))
{
	GAND_INFO_LOG("unloading gandalf module");
	if (__cnet) {
		ud_conn_fini(__cnet);
	}
	if (__cuds) {
		ud_conn_fini(__cuds);
	}
	if (__sym_inot) {
		ud_conn_fini(__sym_inot);
	}
	if (__symu_inot) {
		ud_conn_fini(__symu_inot);
	}
	if (trolfdir) {
		free(trolfdir);
	}
	/* unlink the unix domain socket */
	if (gand_sock_path != NULL) {
		unlink(gand_sock_path);
	}
	fclose(gand_logout);
	GAND_INFO_LOG("gandalf successfully unloaded");
	return;
}

/* dso-gandalf.c ends here */

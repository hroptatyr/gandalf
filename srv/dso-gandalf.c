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

#include "gandalf.h"
#include "nifty.h"

/* get rid of libev guts */
#if defined HARD_INCLUDE_con6ity
# undef DECLF
# undef DECLF_W
# undef DEFUN
# undef DEFUN_W
# define DECLF		static
# define DEFUN		static
# define DECLF_W	static
# define DEFUN_W	static
#endif	/* HARD_INCLUDE_con6ity */
#include "con6ity.h"

#define MOD_PRE		"mod/gandalf"

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


/* connexion<->proto glue */
static int
wr_fin_cb(gand_conn_t ctx)
{
	char *buf = get_fd_data(ctx);
	GAND_DEBUG(MOD_PRE ": finished writing buf %p\n", buf);
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
make_symbol_name(void)
{
	static const char rsym[] = "rolft_symbol";
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
	memcpy(f + idx, rsym, sizeof(rsym) - 1);
	inip = true;
	goto singleton;
}

static void
free_symbol_name(const char *UNUSED(sym))
{
	return;
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
mmap_whole_file(struct mmfb_s *mf, const char *f)
{
	struct stat st[1];

	if (UNLIKELY(stat(f, st) < 0)) {
		return -1;
	} else if (UNLIKELY((mf->fd = open(f, O_RDONLY)) < 0)) {
		return -1;
	}

	/* mmap the file */
	mf->m.buf = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, mf->fd, 0);
	return mf->m.all = mf->m.bsz = st->st_size;
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
match_date1_p(const char *ln, size_t lsz, struct date_rng_s *dr)
{
	const char *dt;
	idate_t idt;

	dt = rawmemchr(ln, '\t');
	dt = rawmemchr(dt + 1, '\t');
	dt = rawmemchr(dt + 1, '\t');

	idt = __to_idate(dt + 1);
	if (idt >= dr->beg && idt <= dr->end) {
		return true;
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

	a = rawmemchr(ln, '\t');
	a = rawmemchr(a + 1, '\t');
	a = rawmemchr(a + 1, '\t');
	a = rawmemchr(a + 1, '\t');
	a = rawmemchr(a + 1, '\t');
	eoa = rawmemchr(++a, '\t');

	if (strncmp(vf->this, a, eoa - a) == 0) {
		/* never try any alternatives */
		vf->nalts = 0;
		return true;
	}
	for (size_t i = 0; i < vf->nalts; i++) {
		if (strncmp(vf->alts[i], a, eoa - a) == 0) {
			GAND_DEBUG("matched %zu-th alt, cancelling\n", i);
			cancel_alts(vf, i);
			return true;
		}
	}
	return false;
}

static bool
match_msg_p(const char *ln, size_t lsz, gand_msg_t msg)
{
	bool res;

	res = msg->ndate_rngs == 0;
	for (size_t i = 0; i < msg->ndate_rngs; i++) {
		if (match_date1_p(ln, lsz, msg->date_rngs + i)) {
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
bang_line(struct mmmb_s *mb, const char *lin, size_t lsz)
{
	char *s;
	char *tmp;

	/* check if we need to resize */
	if (mb->all == 0) {
		size_t ini_sz = (lsz & ~(BUF_INC - 1)) + BUF_INC;
		mb->buf = mmap(NULL, ini_sz, PROT_MEM, MAP_MEM, 0, 0);
		mb->all = ini_sz;
	} else if (mb->bsz + lsz + 1 > mb->all) {
		size_t new = ((mb->bsz + lsz) & ~(BUF_INC - 1)) + BUF_INC;
		mb->buf = mremap(mb->buf, mb->all, new, MREMAP_MAYMOVE);
		mb->all = new;
	}

	/* copy only interesting lines */
	s = rawmemchr(lin, '\t');
	tmp = rawmemchr(s + 1, '\t');
	memcpy(mb->buf + mb->bsz, s + 1, tmp - (s + 1));
	mb->bsz += tmp - (s + 1);

	s = rawmemchr(tmp + 1, '\t');
	tmp = rawmemchr(s + 1, '\t');
	memcpy(mb->buf + mb->bsz, s, tmp - s);
	mb->bsz += tmp - (s + 1);

	s = rawmemchr(tmp + 1, '\t');
	memcpy(mb->buf + mb->bsz, s, lsz - (s - lin));
	mb->bsz += lsz - (s - lin);

	/* finalise the line */
	mb->buf[mb->bsz++] = '\n';
	return;
}

static void
bang_whole_line(struct mmmb_s *mb, const char *lin, size_t lsz)
{
	/* check if we need to resize */
	if (mb->all == 0) {
		size_t ini_sz = (lsz & ~(BUF_INC - 1)) + BUF_INC;
		mb->buf = mmap(NULL, ini_sz, PROT_MEM, MAP_MEM, 0, 0);
		mb->all = ini_sz;
	} else if (mb->bsz + lsz + 1 > mb->all) {
		size_t new = ((mb->bsz + lsz) & ~(BUF_INC - 1)) + BUF_INC;
		mb->buf = mremap(mb->buf, mb->all, new, MREMAP_MAYMOVE);
		mb->all = new;
	}

	/* copy only interesting lines */
	memcpy(mb->buf + mb->bsz, lin, lsz);
	mb->bsz += lsz;

	/* finalise the line */
	mb->buf[mb->bsz++] = '\n';
	return;
}

static uint32_t
get_rolf_id(struct rolf_obj_s *robj)
{
	uint32_t rid = 0U;
	const char *rsym;
	size_t rssz;
	size_t rest;

	/* REPLACE THE LOOKUP PART WITH A PREFIX TREE */
	if (LIKELY(robj->rolf_id > 0)) {
		return robj->rolf_id;
	} else if (grsym.m.buf == NULL) {
		return 0U;
	}

	/* set up */
	rsym = robj->rolf_sym;
	rssz = strlen(rsym);
	for (const char *cand = (rest = grsym.m.bsz, grsym.m.buf);
	     (cand = memmem(cand, rest, rsym, rssz)) != NULL;
	     rest = grsym.m.bsz - (cand - grsym.m.buf)) {
		if (cand == grsym.m.buf || cand[-1] == '\n') {
			/* we've got a prefix match */
			cand = rawmemchr(cand, '\t');
			rid = strtoul(cand + 1, NULL, 10);
			break;
		}
	}
	return rid;
}

static size_t
get_ser(char **buf, gand_msg_t msg)
{
	struct mmmb_s mb = {0};
	struct mmfb_s mf = {.m = {0}, .fd = -1};
	const char *f;
	uint32_t rid;

	/* general checks and get us the lateglu name */
	if (UNLIKELY(msg->nrolf_objs == 0)) {
		return 0UL;
	} else if ((rid = get_rolf_id(msg->rolf_objs)) == 0) {
		return 0UL;
	} else if ((f = make_lateglu_name(rid)) == NULL) {
		return 0UL;
	} else if (mmap_whole_file(&mf, f) < 0) {
		goto out;
	}

	for (size_t idx = 0; idx < mf.m.bsz; ) {
		const char *lin = mf.m.buf + idx;
		char *eol = rawmemchr(lin, '\n');
		size_t lsz = eol - lin;

		if (match_msg_p(lin, lsz, msg)) {
			bang_line(&mb, lin, lsz);
		}
		idx += lsz + 1;
	}

	/* prepare output */
	mmmb_freeze(&mb);
	*buf = mb.buf;
	/* free the resources */
	munmap_all(&mf);
out:
	free_lateglu_name(f);
	return mb.bsz;
}

static const char* __attribute__((noinline))
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
	} else if (mmap_whole_file(&mf, f) < 0) {
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
		     rest = mf.m.bsz - (cand - mf.m.buf)) {
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
			bang_whole_line(&mb, cand, cend - cand);
			cand = cend + 1;
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
		GAND_DEBUG(MOD_PRE ": get_series msg %zu dates  %zu vfs\n",
			   msg->ndate_rngs, msg->nvalflavs);
		len = get_ser(buf, msg);
		break;

	case GAND_MSG_GET_DAT:
		GAND_DEBUG(MOD_PRE ": get_date msg %zu rids\n",
			   msg->nrolf_objs);
		break;

	case GAND_MSG_GET_NFO:
		GAND_DEBUG(MOD_PRE ": get_info msg %zu rids\n",
			   msg->nrolf_objs);
		len = get_nfo(buf, msg);
		break;

	default:
		GAND_DEBUG(MOD_PRE ": unknown message %u\n", msg->hdr.mt);
		break;
	}
	/* free 'im 'ere */
	gand_free_msg(msg);
	return len;
}

/**
 * Take the stuff in MSG of size MSGLEN coming from FD and process it.
 * Return values <0 cause the handler caller to close down the socket. */
DEFUN int
handle_data(gand_conn_t ctx, char *msg, size_t msglen)
{
	gand_ctx_t p = get_fd_data(ctx);
	gand_msg_t umsg;

	GAND_DEBUG(MOD_PRE "/ctx: %p %zu\n", ctx, msglen);
#if defined DEBUG_FLAG
	/* safely write msg to logerr now */
	fwrite(msg, msglen, 1, logout);
#endif	/* DEBUG_FLAG */

	if ((umsg = gand_parse_blob_r(&p, msg, msglen)) != NULL) {
		/* definite success */
		char *buf = NULL;
		size_t len;

		/* serialise, put results in BUF*/
		if ((len = interpret_msg(&buf, umsg))) {
			gand_conn_t wr;

			GAND_DEBUG(
				MOD_PRE ": installing buf wr'er %p %zu\n",
				buf, len);
			/* use the write-soon service */
			wr = write_soon(ctx, buf, len, wr_fin_cb);
			put_fd_data(wr, buf);
			set_conn_flag_munmap(wr);
			put_fd_data(ctx, wr);
			return 0;
		}
		/* kick original context's data */
		put_fd_data(ctx, NULL);
		return 0;

	} else if (/* umsg == NULL && */p == NULL) {
		/* error occurred */
		GAND_DEBUG(MOD_PRE ": ERROR\n");
	} else {
		GAND_DEBUG(MOD_PRE ": need more grub\n");
	}
	put_fd_data(ctx, p);
	return 0;
}
#define HAVE_handle_data

DEFUN void
handle_close(gand_conn_t ctx)
{
	gand_ctx_t p;

	GAND_DEBUG("forgetting about %d\n", get_fd(ctx));
	if ((p = get_fd_data(ctx)) != NULL) {
		/* finalise the push parser to avoid mem leaks */
		gand_msg_t msg = gand_parse_blob_r(&p, ctx, 0);

		if (UNLIKELY(msg != NULL)) {
			/* sigh */
			gand_free_msg(msg);
		}
	}
	put_fd_data(ctx, NULL);
	return;
}
#define HAVE_handle_close

DEFUN int
handle_inot(gand_conn_t ctx, const char *f, const struct stat *UNUSED(st))
{
	/* off with the old guy */
	munmap_all(&grsym);
	/* reinit */
	GAND_DEBUG(MOD_PRE ": building sym table ...");
	if (mmap_whole_file(&grsym, f) < 0) {
		GAND_DBGCONT("failed\n");
		return -1;
	}
	GAND_DBGCONT("done\n");
	return 0;
}
#define HAVE_handle_inot


/* our connectivity cruft */
#if defined HARD_INCLUDE_con6ity
# include "con6ity.c"
#endif	/* HARD_INCLUDE_con6ity */


static void
gand_init_inot(ud_ctx_t ctx, const char *file)
{
	init_stat_watchers(ctx->mainloop, file);
	/* god i'm a hacker */
	handle_inot(NULL, file, NULL);
	return;
}

static int
gand_init_uds_sock(const char **sock_path, ud_ctx_t ctx, void *settings)
{
	volatile int res = -1;

	udcfg_tbl_lookup_s(sock_path, ctx, settings, "sock");
	if (*sock_path != NULL &&
	    (res = conn_listener_uds(*sock_path)) > 0) {
		/* set up the IO watcher and timer */
		init_conn_watchers(ctx->mainloop, res);
	} else {
		/* make sure we don't accidentally delete arbitrary files */
		*sock_path = NULL;
	}
	return res;
}

static int
gand_init_net_sock(ud_ctx_t ctx, void *settings)
{
	volatile int res = -1;
	int port;

	port = udcfg_tbl_lookup_i(ctx, settings, "port");
	if (port &&
	    (res = conn_listener_net(port)) > 0) {
		/* set up the IO watcher and timer */
		init_conn_watchers(ctx->mainloop, res);
	}
	return res;
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
static volatile int gand_sock_net = -1;
static volatile int gand_sock_uds = -1;
/* path to unix domain socket */
static const char *gand_sock_path;

void
init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	GAND_DEBUG(MOD_PRE ": loading ...");

	/* glue to lua settings */
	if ((settings = udctx_get_setting(ctx)) == NULL) {
		GAND_DBGCONT("failed\n");
		return;
	}
	GAND_DBGCONT("\n");
	/* obtain the unix domain sock from our settings */
	gand_sock_uds = gand_init_uds_sock(&gand_sock_path, ctx, settings);
	/* obtain port number for our network socket */
	gand_sock_net = gand_init_net_sock(ctx, settings);
	ntrolfdir = gand_get_trolfdir(&trolfdir, ctx, settings);
	/* inotify the symbol file */
	gand_init_inot(ctx, make_symbol_name());

	GAND_DEBUG(MOD_PRE ": ... loaded (%s)\n", trolfdir);

	/* clean up */
	udctx_set_setting(ctx, NULL);
	return;
}

void
reinit(void *UNUSED(clo))
{
	GAND_DEBUG(MOD_PRE ": reloading ...done\n");
	return;
}

void
deinit(void *clo)
{
	ud_ctx_t ctx = clo;

	GAND_DEBUG(MOD_PRE ": unloading ...");
	deinit_conn_watchers(ctx->mainloop);
	deinit_stat_watchers(ctx->mainloop);
	gand_sock_net = -1;
	gand_sock_uds = -1;
	if (trolfdir) {
		free(trolfdir);
	}
	/* unlink the unix domain socket */
	if (gand_sock_path != NULL) {
		unlink(gand_sock_path);
	}
	GAND_DBGCONT("done\n");
	return;
}

/* dso-gandalf.c ends here */

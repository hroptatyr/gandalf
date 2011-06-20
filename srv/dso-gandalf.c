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

static char *trolfdir;
static size_t ntrolfdir;


/* connexion<->proto glue */
static int
wr_fin_cb(gand_conn_t ctx)
{
	char *buf = get_fd_data(ctx);
	GAND_DEBUG(MOD_PRE ": finished writing buf %p\n", buf);
	return 0;
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
free_lateglu_name(const char *name)
{
	/* just for later when we're reentrant */
	return;
}

static size_t
mmap_whole_file(char **tgt, int *fd, const char *f)
{
	struct stat st[1];

	/* init */
	*fd = -1;
	*tgt = NULL;

	if (UNLIKELY(stat(f, st) < 0)) {
		return 0UL;
	} else if (UNLIKELY((*fd = open(f, O_RDONLY)) < 0)) {
		return 0UL;
	}

	/* mmap the file */
	*tgt = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, *fd, 0);
	return st->st_size;
}

static void
munmap_all(char *buf, size_t bsz, int fd)
{
	if (buf != NULL && bsz > 0UL) {
		munmap(buf, bsz);
	}
	close(fd);
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
		return true;
	}
	for (size_t i = 0; i < vf->nalts; i++) {
		if (strncmp(vf->alts[i], a, eoa - a) == 0) {
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
bang_line(char **buf, size_t *bsz, const char *lin, size_t lsz)
{
	size_t mmbsz = (*bsz & ~(BUF_INC - 1)) + BUF_INC;

	/* check if we need to resize */
	if (*bsz == 0) {
		size_t ini_sz = (lsz & ~(BUF_INC - 1)) + BUF_INC;
		*buf = mmap(NULL, ini_sz, PROT_MEM, MAP_MEM, 0, 0);
	} else if (*bsz + lsz > mmbsz) {
		size_t new = ((*bsz + lsz + 1) & ~(BUF_INC - 1)) + BUF_INC;
		*buf = mremap(*buf, mmbsz, new, MREMAP_MAYMOVE);
	}

	memcpy(*buf + *bsz, lin, lsz);
	(*buf)[*bsz += lsz] = '\n';
	(*bsz)++;
	return;
}

static size_t
get_ser(char **buf, gand_msg_t msg)
{
	const char *f;
	char *fb;
	size_t fsz;
	int fd;
	size_t bsz = 0;

	/* init the bollocks */
	*buf = NULL;
	bsz = 0;

	/* general checks and get us the lateglu name */
	if (UNLIKELY(msg->nrolf_objs == 0)) {
		return 0UL;
	} else if ((f = make_lateglu_name(msg->rolf_objs[0].rolf_id)) == NULL) {
		return 0UL;
	} else if ((fsz = mmap_whole_file(&fb, &fd, f)) == 0) {
		goto out;
	}

	for (size_t idx = 0; idx < fsz; ) {
		const char *lin = fb + idx;
		char *eol = rawmemchr(lin, '\n');
		size_t lsz = eol - lin;

		if (match_msg_p(lin, lsz, msg)) {
			bang_line(buf, &bsz, lin, lsz);
		}
		idx += lsz + 1;
	}

	/* free the resources */
	munmap_all(fb, fsz, fd);
out:
	free_lateglu_name(f);
	return bsz;
}

static size_t
interpret_msg(char **buf, gand_msg_t msg)
{
	size_t len = 0;

	switch (gand_get_msg_type(msg)) {
	case GAND_MSG_GET_SERIES:
		GAND_DEBUG(MOD_PRE ": get_series msg %zu dates  %zu vfs\n",
			   msg->ndate_rngs, msg->nvalflavs);
		len = get_ser(buf, msg);
		break;

	case GAND_MSG_GET_DATE:
		GAND_DEBUG(MOD_PRE ": get_date msg %zu rids\n",
			   msg->nrolf_objs);
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


/* our connectivity cruft */
#if defined HARD_INCLUDE_con6ity
# include "con6ity.c"
#endif	/* HARD_INCLUDE_con6ity */


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

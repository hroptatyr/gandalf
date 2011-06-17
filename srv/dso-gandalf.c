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


/* connexion<->proto glue */
static int
wr_fin_cb(gand_conn_t ctx)
{
	char *buf = get_fd_data(ctx);
	GAND_DEBUG(MOD_PRE ": finished writing buf %p\n", buf);
	free(buf);
	return 0;
}

static size_t
interpret_msg(char **buf, gand_msg_t msg)
{
	size_t len;

	switch (gand_get_msg_type(msg)) {
	case GAND_MSG_GET_SERIES:
		GAND_DEBUG(MOD_PRE ": get_series msg %zu dates\n",
			   msg->ndate_rngs);
		break;

	case GAND_MSG_GET_DATE:
		GAND_DEBUG(MOD_PRE ": get_date msg %zu rids\n",
			   msg->nrolf_objs);
		break;

	default:
		GAND_DEBUG(MOD_PRE ": unknown message %u\n", msg->hdr.mt);
		len = 0;
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

			GAND_DEBUG(MOD_PRE ": installing buf wr'er %p\n", buf);
			wr = write_soon(ctx, buf, len, wr_fin_cb);
			put_fd_data(wr, buf);
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


/* unserding bindings */
static volatile int gand_sock_net = -1;
static volatile int gand_sock_uds = -1;
/* path to unix domain socket */
const char *gand_sock_path;

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

	GAND_DEBUG(MOD_PRE ": ... loaded\n");

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
	/* unlink the unix domain socket */
	if (gand_sock_path != NULL) {
		unlink(gand_sock_path);
	}
	GAND_DBGCONT("done\n");
	return;
}

/* dso-gandalf.c ends here */

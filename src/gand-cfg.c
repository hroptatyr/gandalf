/*** gand-cfg.h -- unified config file reading bishbosh
 *
 * Copyright (C) 2009-2014 Sebastian Freundt
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
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "logger.h"
#include "gand-cfg.h"
#include "nifty.h"

#if !defined PATH_MAX
# define PATH_MAX	(256U)
#endif	/* !PATH_MAX */

/* do me properly */
static const char cfg_glob_prefix[] = "/etc/unserding";

#define GAND_DEFAULT_PORT	8080U

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


cfg_t
gand_read_config(const char *user_cf)
{
	char cfgf[PATH_MAX];
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

void
gand_free_config(cfg_t ctx)
{
	if (ctx != NULL) {
		configger_fini(ctx);
	}
	return;
}

size_t
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

short unsigned int
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
		return (short unsigned int)res;
	}
dflt:
	return GAND_DEFAULT_PORT;
}

/* gand-cfg.c ends here */

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
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <limits.h>
#include <onion/onion.h>
#include <onion/log.h>
#include <tcbdb.h>
#include "configger.h"
#include "logger.h"
#include "fops.h"
#include "nifty.h"

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

#define GAND_DEBUG(args...)
#define GAND_DBGCONT(args...)

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

#define GAND_DEFAULT_PORT	8080U

typedef TCBDB *dict_t;
typedef unsigned int dict_id_t;

static dict_t gsymdb;
static char *trolfdir;
static size_t ntrolfdir;
static char *nfo_fname;


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


static dict_t
make_dict(const char *fn, int oflags)
{
	int omode = BDBOREADER;
	dict_t res;

	if (oflags & O_RDWR) {
		omode |= BDBOWRITER;
	}
	if (oflags & O_CREAT) {
		omode |= BDBOCREAT;
	}

	if (UNLIKELY((res = tcbdbnew()) == NULL)) {
		goto out;
	} else if (UNLIKELY(!tcbdbopen(res, fn, omode))) {
		goto free_out;
	}

	/* success, just return the handle we've got */
	return res;

free_out:
	tcbdbdel(res);
out:
	return NULL;
}

static void
free_dict(dict_t d)
{
	tcbdbclose(d);
	tcbdbdel(d);
	return;
}

static dict_id_t
get_sym(dict_t d, const char sym[static 1U], size_t ssz)
{
	const dict_id_t *rp;
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(d, sym, ssz, rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	}
	return *rp;
}


/* rolf specific */
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

static void
gand_free_config(cfg_t ctx)
{
	if (ctx != NULL) {
		configger_fini(ctx);
	}
	return;
}

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
	return GAND_DEFAULT_PORT;
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


/* rolf <-> onion glue */
static ssize_t
get_ser(onion_response *res, dict_id_t sid)
{
	const char *fn;
	gandfn_t fb;

	if (UNLIKELY((fn = make_lateglu_name(sid)) == NULL)) {
		return -1;
	} else if (UNLIKELY((fb = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	}
	onion_response_set_length(res, fb.fb.z);
	onion_response_write(res, fb.fb.d, fb.fb.z);

	munmap_fn(fb);
	return fb.fb.z;
}

static onion_connection_status
work(void *UNUSED(_), onion_request *req, onion_response *res)
{
	const char _sym[] = "sym";
	const char *sym;
	const char *cmd;
	dict_id_t sid;

	/* definitely leave our mark here */
	onion_response_set_header(res, "Server", gandalf_pkg_string);

	if ((sym = onion_request_get_query(req, _sym)) == NULL) {
		static const char err[] = "no symbol given\n";
		onion_response_set_length(res, sizeof(err) - 1);
		onion_response_write(res, err, sizeof(err) - 1);
		onion_response_set_code(res, HTTP_BAD_REQUEST);
	} else if (!(sid = get_sym(gsymdb, sym, strlen(sym)))) {
		static const char err[] = "symbol not found\n";
		onion_response_set_length(res, sizeof(err) - 1);
		onion_response_write(res, err, sizeof(err) - 1);
		onion_response_set_code(res, HTTP_NOT_FOUND);
	} else if (UNLIKELY((cmd = onion_request_get_path(req)) == NULL)) {
		goto bugger;
	} else if (!strcmp(cmd, "info")) {
		onion_response_printf(res, "%08u\n", sid);
	} else if (!strcmp(cmd, "series")) {
		ssize_t z;

		if (UNLIKELY((z = get_ser(res, sid)) < 0)) {
			goto bugger;
		}
	}
	/* when we get here it's most likely text/plain innit? */
	onion_response_set_header(res, "Content-Type", "text/plain");
	/* we process everything */
	return OCS_PROCESSED;
bugger:
	return OCS_INTERNAL_ERROR;
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


#include "gandalfd.yucc"

int
main(int argc, char *argv[])
{
	/* args */
	yuck_t argi[1U];
	jmp_buf cont;
	onion *o = NULL;
	int daemonisep = 0;
	uint16_t port;
	cfg_t cfg;
	int rc = 0;

	auto void unfold(int UNUSED(_))
	{
		/* no further interruptions please */
		block_sigs();
		onion_listen_stop(o);
		longjmp(cont, 1);
		return;
	}

	/* best not to be signalled for a minute */
	block_sigs();

	/* parse the command line */
	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out0;
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
			goto outd;
		} else if (ret > 0) {
			/* parent process */
			goto outd;
		}
		/* fiddle with onion logging (defaults to stderr) */
		onion_log = onion_log_syslog;
	} else {
		/* fiddle with gandalf logging (default to syslog) */
		gand_log = gand_errlog;
	}

	/* start them log files */
	gand_openlog();

	if ((gsymdb = make_dict("gand_idx2sym.tcb", O_RDONLY)) == NULL) {
		GAND_ERR_LOG("cannot open symbol index file");
		rc = 1;
		goto out0;
	}

	if ((o = onion_new(O_POOL)) == NULL) {
		GAND_ERR_LOG("cannot spawn onion server");
		rc = 1;
		goto out1;
	}

	/* write a pid file? */
	with (const char *pidf) {
		if ((pidf = argi->pidfile_arg) ||
		    (cfg && cfg_glob_lookup_s(&pidf, cfg, "pidfile") > 0)) {
			/* command line has precedence */
			if (write_pidfile(pidf) < 0) {
				GAND_ERR_LOG("cannot write pid file %s", pidf);
			}
		}
	}

	/* get the trolf dir */
	ntrolfdir = gand_get_trolfdir(&trolfdir, cfg);
	nfo_fname = gand_get_nfo_file(cfg);
	port = gand_get_port(cfg);

	/* configure the onion server */
	onion_set_timeout(o, 1000);
	onion_set_hostname(o, "::");
	with (char buf[32U]) {
		snprintf(buf, sizeof(buf), "%hu", port);
		onion_set_port(o, buf);
	}
	with (unsigned int onum = sysconf(_SC_NPROCESSORS_ONLN)) {
		onion_set_max_threads(o, onum);
	}

	onion_set_root_handler(o, onion_handler_new(work, NULL, NULL));

outd:
	/* free cmdline parser goodness */
	yuck_free(argi);
	/* kick the config context */
	gand_free_config(cfg);

	/* main loop */
	if (!setjmp(cont)) {
		/* set up sig handlers */
		signal(SIGINT, unfold);
		signal(SIGTERM, unfold);
		/* set them loose */
		unblock_sigs();
		/* and here we go */
		onion_listen(o);
		/* not reached */
		block_sigs();
	}

	/* free trolfdir and nfo_fname */
	if (LIKELY(trolfdir != NULL)) {
		free(trolfdir);
	}
	if (LIKELY(nfo_fname != NULL)) {
		free(nfo_fname);
	}

	onion_free(o);
out1:
	free_dict(gsymdb);
out0:
	gand_closelog();
	return rc;
}

/* gandalfd-onion.c ends here */

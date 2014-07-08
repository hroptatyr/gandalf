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

static const char v0_main[] = "\
<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <title>gandalf</title>\n\
    <meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\">\n\
    <style type=\"text/css\" media=\"screen\">\n\
      article, aside, details, figcaption, figure,\n\
      footer, header, hgroup, menu, nav, section {\n\
	display: block;\n\
      }\n\
      .search {\n\
	position: relative;\n\
	margin-left: 20%;\n\
	margin-right: 20%;\n\
	margin-top: 1%;\n\
      }\n\
      .search section {\n\
	text-align: center;\n\
      }\n\
      header {\n\
	margin-top: 10%;\n\
	text-align: center;\n\
      }\n\
      footer {\n\
	text-align: center;\n\
      }\n\
      .search input {\n\
        height: 4ex;\n\
        width: 100%;\n\
        margin: 0.7ex 0;\n\
        padding: 0.3ex 10px;\n\
	font-size: x-large;\n\
	font-family: serif;\n\
	color: #555860;\n\
	border: 1px solid;\n\
	border-radius: 10px;\n\
	border-color: #a8acbc #babdcc #c0c3d2;\n\
	-webkit-appearance: textfield;\n\
	-webkit-box-sizing: border-box;\n\
	-moz-box-sizing: border-box;\n\
	box-sizing: border-box;\n\
	box-shadow: inset 0 1px #e5e7ed, 0 1px #fcfcfc;\n\
      }\n\
.search input:focus {\n\
	outline: 0;\n\
	border-color: #66b1ee;\n\
	-webkit-box-shadow: 0 0 2px rgba(85, 168, 236, 0.9);\n\
	box-shadow: 0 0 2px rgba(85, 168, 236, 0.9);\n\
}\n\
\n\
.search input:focus + .search-ac, .search-ac:active {\n\
	display: block;\n\
}\n\
\n\
:-moz-placeholder {\n\
	color: #a7aabc;\n\
	font-weight: 200;\n\
}\n\
\n\
::-webkit-input-placeholder {\n\
	color: #a7aabc;\n\
	font-weight: 200;\n\
	line-height: 14px;\n\
}\n\
\n\
::-webkit-search-decoration,\n\
::-webkit-search-cancel-button {\n\
	-webkit-appearance: none;\n\
}\n\
\n\
    </style>\n\
  </head>\n\
  <body>\n\
    <header>\n\
      <img alt=\"gandalf logo\" src=\"data:image/jpeg;base64,\n\
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\n\
QmCC\" />\n\
    </header>\n\
    <section class=\"search\">\n\
      <form id=\"frm_main\" role=\"search\" method=\"POST\" action=\"javascript:gand_show()\">\n\
	<input type=\"text\" title=\"Search\" autocomplete=\"off\" placeholder=\"Search...\" maxlength=\"2048\" onkeyup=\"gand_ac(this.value)\"></input>\n\
	<ul id=\"res_search_ac\" class=\"search-ac\">\n\
	  <!-- auto filled in -->\n\
	</ul>\n\
      </form>\n\
    </section>\n\
    <section class=\"result\">\n\
      <ul></ul>\n\
    </section>\n\
    <footer>\n\
      &copy; 2010-2014\n\
    </footer>\n\
  </body>\n\
</html>\n\
";

static const char v0_404[] = "\
<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\">\n\
    <style type=\"text/css\" media=\"screen\">\n\
      body {\n\
        position: relative;\n\
        z-index: 0;\n\
        margin-top: 5%;\n\
        text-align: center;\n\
      }\n\
    </style>\n\
  </head>\n\
  <body>\n\
    <img alt=\"404 NOT FOUND\" title=\"404 NOT FOUND\"\n\
      src=\"data:image/jpeg;base64,\n\
iVBORw0KGgoAAAANSUhEUgAAAWQAAABMCAYAAABak83PAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A\n\
/wD/oL2nkwAAAAlwSFlzAAAOwwAADsMBx2+oZAAAAAd0SU1FB94HCAczAoxeMZkAAAExSURBVHja\n\
7dRBDoMgFEXRh/vfM06caGIQBaPJORPTWswvpbckqdkr27UeXqfxfk4+l87n1JPnlUHrcnH96Hln\n\
z9Oa4+rvUzvXPf2+d+d+e397z3dr3q/sb+//bdY5u7ufo+4/PS8ZsZ9LAPgEQQYQZAAEGUCQARBk\n\
AEEGQJABBBkAQQYQZAAEGUCQARBkAEEGQJABBBkAQQYQZAAEGUCQARBkAEEGQJABEGQAQQZAkAEE\n\
GQBBBhBkAAQZQJABEGQAQQZAkAEEGQBBBhBkAAQZQJABEGQAQQZAkAEEGQBBBkCQAQQZAEEGEGQA\n\
BBlAkAEQZABBBkCQAQQZAEEGEGQABBlAkAEQZABBBkCQAQQZAEEGEGQABBkAQQYQZAAEGUCQARBk\n\
AEEGQJABBBkAQQb4rxUwkDGXa8H4QwAAAABJRU5ErkJggg==\" />\n\
    <div>\n\
      404 NOT FOUND\n\
    </div>\n\
  </body>\n\
</html>\n\
";


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
#include "gand-endpoints-gp.c"
#include "gand-outfmts-gp.c"

static gand_ep_t
req_get_endpoint(onion_request *req)
{
	const struct gand_ep_cell_s *epc;
	const char *cmd;
	size_t cmz;

	if (UNLIKELY((cmd = onion_request_get_path(req)) == NULL)) {
		return EP_UNK;
	} else if (UNLIKELY((cmz = strlen(cmd)) == 0U)) {
		return EP_V0_MAIN;
	} else if (UNLIKELY((epc = __gand_ep(cmd, cmz)) == NULL)) {
		return EP_UNK;
	}
	return epc->ep;
}

static gand_of_t
req_get_outfmt(onion_request *req)
{
	const char *acc = onion_request_get_header(req, "Accept");
	gand_of_t of = OF_UNK;
	const char *on;

	if (UNLIKELY(acc == NULL)) {
		return OF_UNK;
	}
	/* otherwise */
	do {
		const struct gand_of_cell_s *ofc;
		size_t acz;

		if ((on = strchr(acc, ',')) == NULL) {
			acz = strlen(acc);
		} else {
			acz = on++ - acc;
		}
		/* check if there's semicolon specs */
		with (const char *sc = strchr(acc, ';')) {
			if (sc && on && sc < on || sc) {
				acz = sc - acc;
			}
		}

		if (LIKELY((ofc = __gand_of(acc, acz)) != NULL)) {
			/* first one wins */
			of = ofc->of;
			break;
		}
	} while ((acc = on));
	return of;
}

static onion_connection_status
work(void *UNUSED(_), onion_request *req, onion_response *res)
{
	static const char *const ctypes[] = {
		[OF_UNK] = "text/plain",
		[OF_JSON] = "application/json",
		[OF_CSV] = "text/csv",
		[OF_HTML] = "text/html",
	};
	gand_ep_t ep;
	gand_of_t of;
	struct rtup_s {
		enum onion_response_codes_e rc;
		gand_of_t of;
		const char *data;
		size_t dlen;
	} rtup;

	/* definitely leave our mark here */
	onion_response_set_header(res, "Server", gandalf_pkg_string);

	/* get output format and endpoint */
	of = req_get_outfmt(req);
	ep = req_get_endpoint(req);

	switch (ep) {
	default:
	case EP_UNK:
		rtup = (struct rtup_s){
			HTTP_NOT_FOUND, OF_HTML, v0_404, sizeof(v0_404) - 1U
		};
		break;
	case EP_V0_INFO:
	case EP_V0_SERIES:
		onion_response_printf(res, "got %u\n", ep);
		break;
	case EP_V0_MAIN:
		rtup = (struct rtup_s){
			HTTP_OK, OF_HTML, v0_main, sizeof(v0_main) - 1U
		};
		break;
	}

	/* set response type */
	onion_response_set_code(res, rtup.rc);
	onion_response_set_header(res, "Content-Type", ctypes[rtup.of]);
	onion_response_set_length(res, rtup.dlen);
	onion_response_write(res, rtup.data, rtup.dlen);

	/* we process everything */
	return OCS_PROCESSED;
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
		onion_log = onion_log_stderr;
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

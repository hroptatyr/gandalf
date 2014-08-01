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

static const char*
make_lateglu_name(dict_oid_t rolf_id)
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

static ssize_t
filter_csv(char *restrict scratch, size_t z, struct rln_s r)
{
	char *sp = scratch;

	/* quick sanity check, should also warm up caches */
	if (UNLIKELY(r.sym.s == NULL)) {
		return 0;
	} else if (UNLIKELY(r.sym.z + 1U/*\t*/ +
			    r.dat.z + 1U/*\t*/ +
			    r.vrb.z + 1U/*\t*/ +
			    r.val.z + 1U/*\n*/ >= z)) {
		return -1;
	}

	/* normally we'd do some filtering here as well */
	;

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
filter_json(char *restrict scratch, size_t z, struct rln_s r)
{
	if (UNLIKELY(r.sym.s == NULL)) {
		return -1;
	}

	/* normally we'd do some filtering here as well */
	;

	return -1;
}


/* rolf <-> onion glue */
typedef enum {
	EP_UNK,
	EP_V0_SERIES,
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
#define EP(_x_)		(_eps_ ## _x_)

static gand_ep_t
__gand_ep(const char *s, size_t z)
{
/* perform a prefix match */

	if (z < sizeof(EP(V0_SERIES)) - 1U) {
		;
	} else if (!memcmp(EP(V0_SERIES), s, sizeof(EP(V0_SERIES)) - 1U)) {
		return EP_V0_SERIES;
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

static gand_httpd_res_t
work_ser(gand_httpd_req_t req)
{
	const char *sym;
	dict_oid_t rid;
	gand_of_t of;

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
			.rd = {DTYP_DATA, errmsg},
		};
	} else if (!(rid = dict_sym2oid(gsymdb, sym, strlen(sym)))) {
		static const char errmsg[] = "Symbol not found\n";

		GAND_INFO_LOG(":rsp [409 Conflict]: Symbol not found");
		return (gand_httpd_res_t){
			.rc = 409U/*CONFLICT*/,
			.ctyp = OF(UNK),
			.clen = sizeof(errmsg)- 1U,
			.rd = {DTYP_DATA, errmsg},
		};
	} else goto yacka;

yacka:;
	const char *fn;
	gandfn_t fb;
	ssize_t(*filter)(char *restrict, size_t, struct rln_s ln);
	gand_gbuf_t gb;

	/* otherwise we've got some real yacka to do */
	if (UNLIKELY((fn = make_lateglu_name(rid)) == NULL)) {
		goto interr;
	} else if (UNLIKELY((fb = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto interr;
	}

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
		struct rln_s ln;
		ssize_t z;

		if (UNLIKELY((eol = memchr(bol, '\n', max)) == NULL)) {
			eol = bol + max;
		}
		/* snarf the line, v0 format, zero copy */
		ln = snarf_rln(bol, eol - bol);
	filt:
		/* filter, maybe */
		z = filter(proto + tot, sizeof(proto) - tot, ln);
		if (UNLIKELY(z < 0)) {
			/* flush buffer */
			if (UNLIKELY(gand_gbuf_write(gb, proto, tot) < 0)) {
				GAND_ERR_LOG("cannot write to gbuf");
				goto interr_unmap;
			}
			tot = 0U;
			goto filt;
		}
		tot += z;
		i += eol - bol;
	}
	/* flush */
	if (UNLIKELY(gand_gbuf_write(gb, proto, tot) < 0)) {
		goto interr_unmap;
	}

	munmap_fn(fb);
	GAND_INFO_LOG(":rsp [200 OK]: series %08u", rid);
	return (gand_httpd_res_t){
		.rc = 200U/*OK*/,
		.ctyp = _ofs[of],
		.clen = CLEN_UNKNOWN,
		.rd = {DTYP_GBUF, gb},
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
	case EP_V0_MAIN:
		GAND_INFO_LOG(":rsp [200 OK]");
		return (gand_httpd_res_t){
			.rc = 200U/*OK*/,
			.ctyp = "text/html",
			.clen = CLEN_UNKNOWN,
			.rd = {DTYP_FILE, "index.html"},
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
		.rd = {DTYP_FILE, "404.html"},
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
	close_dict(gsymdb);
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
	yuck_t argi[1U];
	gand_httpd_t h;
	int daemonisep = 0;
	short unsigned int port;
	/* paths and files */
	const char *pidf;
	const char *wwwd;
	const char dictf[] = "gand_idx2sym.tcb";
	/* inotify watcher */
	ev_stat dict_watcher;
	cfg_t cfg;
	int rc = 0;

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
	} else {
		/* fiddle with gandalf logging (default to syslog) */
		gand_log = gand_errlog;
	}

	/* start them log files */
	gand_openlog();

	if ((gsymdb = open_dict(dictf, O_RDONLY)) == NULL) {
		GAND_ERR_LOG("cannot open symbol index file");
		rc = 1;
		goto out0;
	}

	/* write a pid file? */
	if ((pidf = argi->pidfile_arg) ||
	    (cfg && cfg_glob_lookup_s(&pidf, cfg, "pidfile") > 0)) {
		/* command line has precedence */
		if (write_pidfile(pidf) < 0) {
			GAND_ERR_LOG("cannot write pid file `%s': %s",
				     pidf, strerror(errno));
			rc = 1;
			goto out1;
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
			goto out1;
		} else if (stat(wwwd, &st) < 0) {
			GAND_ERR_LOG("cannot access wwwdir `%s': %s",
				     wwwd, strerror(errno));
			rc = 1;
			goto out1;
		} else if (!S_ISDIR(st.st_mode)) {
			GAND_ERR_LOG("wwwdir `%s' not a directory", wwwd);
			rc = 1;
			goto out1;
		} else if (access(wwwd, X_OK) < 0) {
			GAND_ERR_LOG("cannot access wwwdir `%s': %s",
				     wwwd, strerror(errno));
			rc = 1;
			goto out1;
		}
	}

	/* get the trolf dir */
	ntrolfdir = gand_get_trolfdir(&trolfdir, cfg);
	nfo_fname = gand_get_nfo_file(cfg);
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
		goto out2;
	}

	/* we need an inotify on the dict file */
	with (void *loop = ev_default_loop(EVFLAG_AUTO)) {
		ev_stat_init(&dict_watcher, stat_cb, dictf, 0);
		ev_stat_start(EV_A_ &dict_watcher);
	}

outd:
	/* free cmdline parser goodness */
	yuck_free(argi);
	/* kick the config context */
	gand_free_config(cfg);

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

	/* away with the http */
	free_gand_httpd(h);
out2:
	/* free trolfdir and nfo_fname */
	if (LIKELY(trolfdir != NULL)) {
		free(trolfdir);
	}
	if (LIKELY(nfo_fname != NULL)) {
		free(nfo_fname);
	}

out1:
	close_dict(gsymdb);
	if (pidf != NULL) {
		(void)unlink(pidf);
	}
out0:
	gand_closelog();
	return rc;
}

/* gandalfd.c ends here */

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
#include <sys/mman.h>
#include <sys/stat.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */

#include "gandalf.h"
#include "logger.h"
#include "configger.h"
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


#define GLOB_CFG_PRE	"/etc/unserding"
#if !defined MAX_PATH_LEN
# define MAX_PATH_LEN	64
#endif	/* !MAX_PATH_LEN */

/* do me properly */
static const char cfg_glob_prefix[] = GLOB_CFG_PRE;

#if defined USE_LUA
static const char cfg_file_name[] = "gandalf.lua";

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
	lua_config_init(&cfg);

	/* we prefer the user's config file, then fall back to the
	 * global config file if that's not available */
	if (user_cf && read_lua_config(cfg, user_cf)) {
		GAND_DBGCONT("done\n");
		return cfg;
	}

	gand_expand_user_cfg_file_name(cfgf);
	if (read_lua_config(cfg, cfgf)) {
		GAND_DBGCONT("done\n");
		return cfg;
	}
	/* otherwise there must have been an error */
	gand_expand_glob_cfg_file_name(cfgf);
	if (read_lua_config(cfg, cfgf)) {
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
		lua_config_deinit(ctx);
	}
	return;
}
#endif	/* USE_LUA */


/* callbacks */
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


/* server helpers */
static int
daemonise(void)
{
	int fd;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return false;
	case 0:
		break;
	default:
		GAND_NOTI_LOG("Successfully bore a squaller: %d\n", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return false;
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
	/* args */
	struct gengetopt_args_info argi[1];
	/* our take on args */
	bool daemonisep = false;
	bool prefer6p = false;
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
		prefer6p |= cfg_glob_lookup_b(cfg, "prefer_ipv6");
	}

	/* run as daemon, do me properly */
	if (daemonisep) {
		daemonise();
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

	/* free cmdline parser goodness */
	cmdline_parser_free(argi);
	/* kick the config context */
	gand_free_config(cfg);

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

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

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	GAND_NOTI_LOG("shutting down unserdingd\n");

	/* destroy the default evloop */
	ev_default_destroy();

	/* close our log output */	
	fflush(gand_logout);
	fclose(gand_logout);
	gand_closelog();
	/* unloop was called, so exit */
	return 0;
}

/* gandalfd.c ends here */

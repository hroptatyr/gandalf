/*** gandaux.c -- gandalf cli auxiliary helper
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
#if !defined _GNU_SOURCE
/* for getline() innit */
# define _GNU_SOURCE
#endif	/* _GNU_SOURCE */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include "gand-dict.h"
#include "nifty.h"

typedef unsigned int dict_id_t;

static char *idxf = "gand_idx2sym.tcb";


static __attribute__((format(printf, 1, 2))) void
serror(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static dict_si_t
get_idx_ln(FILE *f)
{
/* uses static state */
	static char *line;
	static size_t llen;
	static FILE *cf;
	ssize_t nrd;
	dict_si_t res = {};

	if (UNLIKELY(f == NULL)) {
		goto out;
	} else if (cf != f) {
		if (line != NULL) {
			free(line);
		}
		line = NULL;
		llen = 0U;
		if ((cf = f) == NULL) {
			goto out;
		}
	}

	if ((nrd = getline(&line, &llen, f)) > 0) {
		char *on;

		if ((res.sid = strtoul(line, &on, 10)) == 0U) {
			;
		} else if (*on != '\t') {
			res.sid = 0U;
		} else {
			/* valid line it seems */
			line[nrd - 1] = '\0';
			res.sym = on + 1;
		}
	}
out:
	return res;
}

static dict_oid_t
add_sym(dict_t d, const char *sym)
{
/* add SYM with id SID (or if 0 generate one) and return the SID. */
	dict_oid_t sid;

	if ((sid = dict_get_sym(d, sym))) {
		/* ok, nothing to do */
		;
	} else if (UNLIKELY(!(sid = dict_next_oid(d)))) {
		/* huh? */
		;
	/* finally just assoc SYM with SID */
	} else if (UNLIKELY(!dict_put_sym(d, sym, sid))) {
		/* grrrr */
		sid = 0U;
	}
	return sid;
}


#include "gandaux.yucc"

static int
cmd_addget(const struct yuck_cmd_get_s argi[static 1U], bool addp)
{
	int oflags;
	dict_t d;
	int rc = 0;

	if (addp) {
		oflags = O_RDWR | O_CREAT;
	} else {
		oflags = O_RDONLY;
	}

	if ((d = open_dict(idxf, oflags)) == NULL) {
		fputs("cannot open symbol index file\n", stderr);
		rc = 1;
		goto out;
	}

	if (argi->nargs) {
		for (unsigned int i = 0U; i < argi->nargs; i++) {
			const char *sym = argi->args[i];
			dict_id_t id;

			if (addp && !(id = add_sym(d, sym))) {
				fprintf(stderr, "\
cannot add symbol `%s'\n", sym);
			} else if (!addp && !(id = dict_get_sym(d, sym))) {
				fprintf(stderr, "\
no symbol `%s' in index file\n", sym);
			}
			printf("%08u\n", id);
		}
	} else {
		/* get symlist from stdin */
		char *line = NULL;
		size_t llen = 0UL;
		ssize_t nrd;

		while ((nrd = getline(&line, &llen, stdin)) > 0) {
			const char *sym = line;
			size_t ssz = nrd - 1U;
			dict_id_t id;

			line[ssz] = '\0';
			if (addp && !(id = add_sym(d, sym))) {
				fprintf(stderr, "\
cannot add symbol `%s'\n", sym);
			} else if (!addp && !(id = dict_get_sym(d, sym))) {
				fprintf(stderr, "\
no symbol `%s' in index file\n", sym);
			}
			printf("%08u\n", id);
		}
	}

	/* get ready to bugger off */
	close_dict(d);
out:
	return rc;
}

static int
cmd_build(const struct yuck_cmd_build_s argi[static 1U])
{
	char tmpf[] = ".gand_idx2sym.XXXXXXXX";
	const int oflags = O_RDWR | O_TRUNC | O_CREAT;
	int fd;
	dict_t d;
	int rc = 0;

	if (!argi->nargs) {
		yuck_auto_help((const void*)argi);
		rc = 1;
		goto out;
	} else if ((fd = mkstemp(tmpf)) < 0) {
		serror("cannot creat temporary index file `%s'", tmpf);
		rc = 1;
		goto out;
	} else if ((d = open_dict(tmpf, oflags)) == NULL) {
		serror("cannot create temporary index file `%s'", tmpf);
		rc = 1;
		goto out;
	}

	/* close mkstemp's descriptor */
	close(fd);
	
	with (const char *fn = argi->args[0U]) {
		dict_id_t max = 0U;
		FILE *f;

		if ((f = fopen(fn, "r")) == NULL) {
			rc = 1;
			break;
		}

		for (dict_si_t ln; (ln = get_idx_ln(f)).sid;) {
			if (!dict_put_sym(d, ln.sym, ln.sid)) {
				/* ok, fuck that then */
				;
			} else if (ln.sid > max) {
				max = ln.sid;
			}
		}

		fclose(f);

		/* make sure the maximum index value is recorded */
		dict_set_next_oid(d, max);
	}

	/* get ready to bugger off */
	close_dict(d);

	if (rc == 0) {
		/* rename (atomically) to actual file name */
		rc = rename(tmpf, idxf);
	} else {
		(void)unlink(tmpf);
	}
out:
	return rc;
}

static int
cmd_dump(const struct yuck_cmd_dump_s UNUSED(argi[static 1U]))
{
	dict_t d;
	int rc = 0;

	if ((d = open_dict(idxf, O_RDONLY)) == NULL) {
		fputs("cannot open symbol index file\n", stderr);
		rc = 1;
		goto out;
	}

	/* just iterate (coroutine with static state) */
	for (dict_si_t si; (si = dict_iter(d)).sid;) {
		printf("%s\t%08u\n", si.sym, si.sid);
	}

	/* and out we are */
	close_dict(d);
out:
	return rc;
}

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	bool addp = false;
	int rc = 0;

	/* parse the command line */
	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (argi->index_file_arg) {
		idxf = argi->index_file_arg;
	}

	switch (argi->cmd) {
	case GANDAUX_CMD_NONE:
	default:
		fputs("Unknown command\n\n", stderr);
		/* print help */
		yuck_auto_help(argi);
		rc = 1;
		break;
	case GANDAUX_CMD_ADD:
		addp = true;
	case GANDAUX_CMD_GET:
		rc = cmd_addget((const void*)argi, addp);
		break;
	case GANDAUX_CMD_BUILD:
		rc = cmd_build((const void*)argi);
		break;
	case GANDAUX_CMD_DUMP:
		rc = cmd_dump((const void*)argi);
		break;
	}

out:
	yuck_free(argi);
	return rc;
}

/* gandaux.c ends here */

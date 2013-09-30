/*** gandalf.c -- gandalf cli helper
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
# define _GNU_SOURCE
#endif	/* _GNU_SOURCE */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <tcbdb.h>
#include <fcntl.h>
#include "nifty.h"

typedef TCBDB *dict_t;

typedef unsigned int dict_id_t;

typedef struct dict_si_s dict_si_t;

struct dict_si_s {
	dict_id_t sid;
	const char *sym;
};


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

#define SID_SPACE	"\x1d"
#define SYM_SPACE	"\x20"

static dict_id_t
next_id(dict_t d)
{
	static const char sid[] = SID_SPACE;
	int res;

	if (UNLIKELY((res = tcbdbaddint(d, sid, sizeof(sid), 1)) <= 0)) {
		return 0U;
	}
	return (dict_id_t)res;
}

static dict_id_t
add_sym(dict_t d, const char *sym, ...)
{
/* add SYM with id SID (or if 0 generate one) and return the SID. */
	va_list ap;
	dict_id_t sid;

	va_start(ap, sym);
	sid = va_arg(ap, dict_id_t);
	va_end(ap);

	if (!sid) {
		sid = next_id(d);
	}
	tcbdbput(d, sym, strlen(sym), &sid, sizeof(sid));
	return sid;
}

static dict_si_t
dict_iter(dict_t d)
{
/* uses static state */
	static BDBCUR *c;
	dict_si_t res;
	const void *vp;
	int z[1U];

	if (UNLIKELY(c == NULL)) {
		c = tcbdbcurnew(d);
		tcbdbcurjump(c, SYM_SPACE, sizeof(SYM_SPACE));
	}

	if (UNLIKELY((vp = tcbdbcurval3(c, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(int)) {
		goto null;
	}
	/* otherwise fill res */
	res.sid = *(const int*)vp;

	if (UNLIKELY((vp = tcbdbcurkey3(c, z)) == NULL)) {
		goto null;
	}
	/* or fill */
	res.sym = vp;
	/* also iterate to the next thing */
	tcbdbcurnext(c);
	return res;

null:
	if (LIKELY(c != NULL)) {
		tcbdbcurdel(c);
	}
	c = NULL;
	return (dict_si_t){};
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "gandalf.xh"
#include "gandalf.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

static int
cmd_build(struct gand_args_info argi[static 1U])
{
	static const char usage[] = "\
Usage: gandalf build IDX2SYM_FILE\n";
	const int oflags = O_RDWR | O_CREAT;
	dict_t d;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usage, stderr);
		res = 1;
		goto out;
	} else if ((d = make_dict("gand_idx2sym.tcb", oflags)) == NULL) {
		fputs("cannot create dict file\n", stderr);
		res = 1;
		goto out;
	}

	with (const char *fn = argi->inputs[1U]) {
		FILE *f;

		if ((f = fopen(fn, "r")) == NULL) {
			res = 1;
			break;
		}

		for (dict_si_t ln; (ln = get_idx_ln(f)).sid;) {
			add_sym(d, ln.sym, ln.sid);
		}

		fclose(f);
	}

	/* get ready to bugger off */
	free_dict(d);
out:
	return res;
}

static int
cmd_dump(struct gand_args_info argi[static 1U])
{
	static const char usage[] = "\
Usage: gandalf dump IDX_FILE\n";
	const char *fn;
	dict_t d;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usage, stderr);
		res = 1;
		goto out;
	} else if ((fn = argi->inputs[1U]) == NULL) {
		fputs(usage, stderr);
		res = 1;
		goto out;
	} else if ((d = make_dict(argi->inputs[1U], O_RDONLY)) == NULL) {
		fprintf(stderr, "cannot open dict file %s\n", fn);
		res = 1;
		goto out;
	}

	/* just iterate (coroutine with static state) */
	for (dict_si_t si; (si = dict_iter(d)).sid;) {
		printf("%s\t%08u\n", si.sym, si.sid);
	}

	/* and out we are */
	free_dict(d);
out:
	return res;
}

int
main(int argc, char *argv[])
{
	struct gand_args_info argi[1];
	int res = 0;

	/* parse the command line */
	if (gand_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1U) {
		gand_parser_print_help();
		res = 1;
		goto out;
	}

	with (const char *cmd = argi->inputs[0U]) {
		if (!strcmp(cmd, "build")) {
			res = cmd_build(argi);
		} else if (!strcmp(cmd, "dump")) {
			res = cmd_dump(argi);
		} else {
			/* print help */
			fprintf(stderr, "Unknown command `%s'\n\n", cmd);
			gand_parser_print_help();
			res = 1;
			break;
		}
	}

out:
	gand_parser_free(argi);
	return res;
}

/* gandalfd.c ends here */

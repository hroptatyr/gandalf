/*** clidalf.c -- just a command line version of what the server should be */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include "gand-dict.h"
#include "nifty.h"
#include "fops.h"

static char *trolfdir = "/var/scratch/freundt/trolf";
static size_t trolfdiz = sizeof("/var/scratch/freundt/trolf") - 1U/*\nul*/;
static dict_t gsymdb;

#if !defined PATH_MAX
# define PATH_MAX	(256U)
#endif	/* PATH_MAX */


static __attribute__((format(printf, 1, 2))) void
error(const char *fmt, ...)
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

static const char*
make_lateglu_name(dict_oid_t rolf_id)
{
	static const char glud[] = "show_lateglu/";
	static char f[PATH_MAX];
	size_t idx;

	/* construct the path */
	memcpy(f, trolfdir, (idx = trolfdiz));
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


#include "clidalf.yucc"

static int
cmd_show(const struct yuck_cmd_show_s argi[static 1U])
{
	for (size_t i = 0U; i < argi->symbol_nargs; i++) {
		const char *sym = argi->symbol_args[i];
		const size_t ssz = strlen(sym);
		dict_oid_t rid;
		const char *fn;
		gandfn_t fb;

		if (!(rid = dict_sym2oid(gsymdb, sym, ssz))) {
			errno = 0;
			error("symbol not found: %s\n", sym);
			continue;
		} else if (UNLIKELY((fn = make_lateglu_name(rid)) == NULL)) {
			error("\
Error: cannot construct lateglu file name: %s  (%08u)\n", sym, rid);
			continue;
		} else if (UNLIKELY((fb = mmap_fn(fn, O_RDONLY)).fd < 0)) {
			error("\
Error: cannot access lateglu file: %s  (%08u)\n", sym, rid);
			continue;
		}

		/* apply filters */
		;

		/* show results */
		;

		/* and close the bugger again */
		munmap_fn(fb);
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	/* args */
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out0;
	}

	/* get trolfdir or use default */
	if (argi->trolfdir_arg) {
		trolfdir = argi->trolfdir_arg;
		trolfdiz = strlen(trolfdir);
	}

	if ((gsymdb = open_dict("gand_idx2sym.tcb", O_RDONLY)) == NULL) {
		error("cannot open symbol index file");
		rc = 1;
		goto out0;
	}


	switch (argi->cmd) {
	default:
		break;
	case CLIDALF_CMD_SHOW:
		rc = cmd_show((const void*)argi);
		break;
	}

	close_dict(gsymdb);
out0:
	yuck_free(argi);
	return rc;
}

/* clidalf.c ends here */

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

static char *trolfdir = "/var/scratch/freundt/trolf";
static dict_t gsymdb;


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


#include "clidalf.yucc"

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
	}

	if ((gsymdb = open_dict("gand_idx2sym.tcb", O_RDONLY)) == NULL) {
		error("cannot open symbol index file");
		rc = 1;
		goto out0;
	}


	switch (argi->cmd) {
	default:
		break;
	}

	close_dict(gsymdb);
out0:
	yuck_free(argi);
	return rc;
}

/* clidalf.c ends here */

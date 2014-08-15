/*** gandqry.c -- example gandalf client using the gandapi */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
/* our stuff */
#include "gandapi.h"

#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */

static size_t npkt = 0;

static int
cb(gand_res_t UNUSED(res), void *UNUSED(clo))
{
	npkt++;
	return 0;
}


#include "gandqry.yucc"

int
main(int argc, char *argv[])
{
	/* args */
	yuck_t argi[1U];
	gand_ctx_t g;
	int timeout = 2000;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		exit(1);
	}

	if (argi->timeout_arg != NULL) {
		timeout = strtoul(argi->timeout_arg, NULL, 0);
	}

	if ((g = gand_open(argi->service_arg, timeout)) == NULL) {
		perror("gand_open()");
		rc = 1;
		goto bugger_off;
	}
	for (size_t i = 0U; i < argi->nargs; i++) {
		npkt = 0;
		gand_get_series(g, argi->args[i], cb, NULL);
		fprintf(stdout, "number of packets %zu\n", npkt);
	}
	gand_close(g);

bugger_off:
	yuck_free(argi);
	return rc;
}

/* gandqry.c ends here */

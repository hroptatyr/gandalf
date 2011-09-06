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


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "gandqry-clo.h"
#include "gandqry-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	/* args */
	struct gengetopt_args_info argi[1];
	gand_ctx_t g;

	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	g = gand_open(argi->service_arg, argi->timeout_arg);
	for (size_t i = 0; i < argi->inputs_num; i++) {
		npkt = 0;
		gand_get_series(
			g, argi->inputs[i],
			argi->valflav_arg, argi->valflav_given,
			cb, NULL);
		fprintf(stdout, "number of packets %zu\n", npkt);
	}
	gand_close(g);

	cmdline_parser_free(argi);
	return 0;
}

/* gandqry.c ends here */

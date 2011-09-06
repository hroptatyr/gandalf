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

#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "gandqry-clo.h"
#include "gandqry-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, const char *argv[])
{
	gand_ctx_t g;
	const char *vf[] = {{"open"}, {"high"}, {"low"}, {"close"}};

	for (size_t i = 0; i < 100; i++) {
		npkt = 0;
		g = gand_open("gonzo:8624", 2000);
		gand_get_series(g, "988006@comd", vf, 4, cb, NULL);
		gand_close(g);
		fprintf(stdout, "number of packets %zu\n", npkt);
	}

	return 0;
}

/* gandqry.c ends here */

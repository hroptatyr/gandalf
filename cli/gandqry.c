#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
/* our stuff */
#include "gandapi.h"

static int
cb(gand_res_t res, void *clo)
{
	fputs("packet\n", stderr);
	return 0;
}

int
main(int argc, const char *argv[])
{
	gand_ctx_t g;

	g = gand_open("chantico:8623", 2000);
	gand_get_series(g, "USD@ecb", NULL, 0, cb, NULL);
	gand_close(g);
	return 0;
}

/* gandqry.c ends here */

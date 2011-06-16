#include <string.h>
#include "gandalf.h"
#include <nifty.h>

extern int __parse(gand_msg_t msg, const char *s, size_t l);


void
gand_free_msg(gand_msg_t msg)
{
	free(msg);
	return;
}

gand_msg_t
gand_parse_blob_r(gand_ctx_t *ctx, const char *buf, size_t bsz)
{
	gand_msg_t res = NULL;

	if (UNLIKELY(bsz == 0)) {
		return res;
	}

	res = calloc(1, sizeof(*res));
	if (__parse(res, buf, bsz) < 0) {
		;
	}

	*ctx = NULL;
	return res;
}

/* gand_msg.c ends here */

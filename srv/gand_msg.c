#include <string.h>
#include "gandalf.h"
#include <nifty.h>

gand_msg_t
gand_parse_blob_r(gand_ctx_t *ctx, const char *buf, size_t bsz)
{
	gand_msg_t res = NULL;

	if (UNLIKELY(bsz == 0)) {
		return res;
	}

	if (strncmp(buf, "get_series", bsz)) {
		res = calloc(1, sizeof(*res));
		gand_set_msg_type(res, GAND_MSG_GET_SERIES);
	} else if (strncmp(buf, "get_date", bsz)) {
		res = calloc(1, sizeof(*res));
		gand_set_msg_type(res, GAND_MSG_GET_DATE);
	}
	*ctx = NULL;
	return res;
}

void
gand_free_msg(gand_msg_t msg)
{
	free(msg);
	return;
}

/* gand_msg.c ends here */

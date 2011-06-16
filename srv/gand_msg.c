#include <string.h>
#include "gandalf.h"
#include "nifty.h"

#define MSG_PRE		"gandmsg"

extern int __parse(gand_msg_t msg, const char *s, size_t l);


void
gand_handle_msg(gand_msg_t msg)
{
	switch (gand_get_msg_type(msg)) {
	case GAND_MSG_GET_SERIES:
		GAND_DEBUG(MSG_PRE ": get_series msg\n");
		break;

	case GAND_MSG_GET_DATE:
		GAND_DEBUG(MSG_PRE ": get_date msg\n");
		break;

	default:
		GAND_DEBUG(MSG_PRE ": unknown message %u\n", msg->hdr.mt);
		break;
	}
	return;
}


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

#include <string.h>
#include "gandalf.h"
#include "nifty.h"
#include "gand_msg-parser.h"
#include "gand_msg-private.h"

#define MSG_PRE		"gandmsg"


void
gand_free_msg(gand_msg_t msg)
{
	unsize_rolf_objs(msg);
	unsize_date_rngs(msg);
	unsize_valflavs(msg);
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

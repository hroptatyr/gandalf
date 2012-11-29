#include <string.h>
#include "gandalf.h"
#include "nifty.h"
#include "gand_msg-parser.h"
#include "gand_msg-private.h"

#define MSG_PRE		"gandmsg"

static struct gand_msg_s msg_pool[16];
static size_t msg_pidx = 0;


/* http parser */
static int
__parse_http(gand_msg_t msg, const char *req, size_t rsz)
{
	return -1;
}


void
gand_free_msg(gand_msg_t msg)
{
	unsize_rolf_objs(msg);
	unsize_date_rngs(msg);
	unsize_valflavs(msg);
	if (LIKELY(msg >= msg_pool && msg < msg_pool + countof(msg_pool))) {
		/* yay, no need to free stuff
		 * we memset the bugger though so it's clean for later use */
		memset(msg, 0, sizeof(*msg));
		/* oh and since we know this one's free now
		 * set the pool index to this guy so it's available next time
		 * we call __make_msg() */
		msg_pidx = msg - msg_pool;
	} else {
		/* was probably calloc'd then */
		free(msg);
	}
	return;
}

static size_t
__next_msg(void)
{
	for (size_t i = msg_pidx + 1; i < countof(msg_pool); i++) {
		if (msg_pool[i].hdr.mt == GAND_MSG_UNK) {
			return i;
		}
	}
	for (size_t i = 0; i < msg_pidx; i++) {
		if (msg_pool[i].hdr.mt == GAND_MSG_UNK) {
			return i;
		}
	}
	return countof(msg_pool);
}

static gand_msg_t
__make_msg(void)
{
	gand_msg_t res = NULL;

	/* check the pool first */
	if (LIKELY(msg_pidx < countof(msg_pool))) {
		res = msg_pool + msg_pidx;
		msg_pidx = __next_msg();
	}
	return res;
}

gand_msg_t
gand_parse_blob(gand_ctx_t *ctx, const char *buf, size_t bsz)
{
	gand_msg_t res;
	int rc;
	const char *p;

	/* for the side effect */
	*ctx = NULL;

	if (UNLIKELY(bsz <= 4)) {
		return NULL;
	}

	/* just in case, we ask the if tree below to reset this to NULL
	 * if there's some form of mistake */
	res = __make_msg();

	if ((p = memmem(buf, bsz, "GET ", 4)) != NULL) {
		/* http request */
		rc = __parse_http(res, p + 4, bsz - (p - buf) - 4);
	} else {
		/* could be classic gandalf command syntax */
		rc = __parse(res, buf, bsz);
	}

	if (UNLIKELY(rc < 0)) {
		gand_free_msg(res);
		return NULL;
	}

	/* all clear, hand 'im out */
	return res;
}

/* gand_msg.c ends here */

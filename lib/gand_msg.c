#include <string.h>
#include "gandalf.h"
#include "nifty.h"
#include "gand_msg-parser.h"
#include "gand_msg-private.h"

#define MSG_PRE		"gandmsg"

static struct gand_msg_s msg_pool[16];
static size_t msg_pidx = 0;


/* http parser */
#include "gand_msg-select.c"

static void
__parse_select(gand_msg_t msg, const char *sel, const size_t ssz)
{
	for (const char *p = sel, *q; p < sel + ssz; p = q + 1) {
		const struct __select_s *tmp;

		if (UNLIKELY((q = memchr(p, ',', ssz - (p - sel))) == NULL)) {
			q = sel + ssz;
		}
		if (LIKELY((tmp = __snarf_select(p, q - p)) != NULL)) {
			msg->sel |= tmp->val;
		}
	}
	return;
}

static void
__parse_filter_vf(gand_msg_t msg, const char *flt, const size_t fsz)
{
	struct valflav_s *vf;

	for (const char *p = flt, *q; p < flt + fsz; p = q + 1) {
		if (UNLIKELY((q = memchr(p, '/', fsz - (p - flt))) == NULL)) {
			q = flt + fsz;
		}
		if (p == flt) {
			/* first item in the list */
			resize_valflavs(msg);
			vf = msg->valflavs + msg->nvalflavs++;
			vf->this = strndup(p, q - p);
		} else {
			/* ... alternatives, vf should still be valid */
			resize_alts(vf);
			vf->alts[vf->nalts++] = strndup(p, q - p);
		}
	}
}

static void
__parse_filter(gand_msg_t msg, const char *flt, const size_t fsz)
{
	for (const char *p = flt, *q; p < flt + fsz; p = q + 1) {
		if (UNLIKELY((q = memchr(p, ',', fsz - (p - flt))) == NULL)) {
			q = flt + fsz;
		}
		/* bang a new valflav, and indirect to parsing alternatives */
		__parse_filter_vf(msg, p, q - p);
	}
	return;
}

static int
__parse_http(gand_msg_t msg, const char *req, size_t rsz)
{
/* we assume we get the GET / bit
 * we can parse:
 * GET /series?sym=foo  or GET /series?rid=14
 * GET /info?sym=bar  or GET /info?rid=13 
 *
 * get_series "NYMEX_HRC_201011@cme" --select sym,d,vf,v -i --filter stl
 * becomes
 * sym=NYMEX_HRC_201011@cme&select=sym,d,vf,v&ignore-case&filter=stl */
	static const char svc_ser[] = "/series?";
	static const size_t svz_ser = sizeof(svc_ser) - 1U;
	static const char svc_nfo[] = "/info?";
	static const size_t svz_nfo = sizeof(svc_nfo) - 1U;
	const char *eol;
	const char *svc;
	const char *arg;

	if (UNLIKELY((eol = memchr(req, '\n', rsz)) == NULL)) {
		/* weirdo message, don't bother */
		return -1;
	}

	/* consider the range [req + 4, eol) and look for services */
	if ((svc = memmem(req, eol - req, svc_ser, svz_ser)) != NULL) {
		gand_set_msg_type(msg, GAND_MSG_GET_SER);
		arg = svc + svz_ser;
	} else if ((svc = memmem(req, eol - req, svc_nfo, svz_nfo)) != NULL) {
		gand_set_msg_type(msg, GAND_MSG_GET_NFO);
		arg = svc + svz_nfo;
	} else {
		/* don't worry about it */
		return -1;
	}

	/* we want ?sym=XYZ or ?rid=NNN */
	{
		static const char arg_rid[] = "rid=";
		static const size_t arz_rid = sizeof(arg_rid) - 1U;
		static const char arg_sym[] = "sym=";
		static const size_t arz_sym = sizeof(arg_sym) - 1U;

#define R(p)	(eol - (p))
		/* look for sym='s */
		for (const char *p = arg, *eoa;
		     (p = memmem(p, R(p), arg_sym, arz_sym)) != NULL &&
			     ((eoa = memchr(p, '&', R(p))) != NULL ||
			      (eoa = memchr(p, ' ', R(p))) != NULL);
		     p = eoa + 1) {
			resize_rolf_objs(msg);
			msg->rolf_objs[msg->nrolf_objs++].rolf_sym =
				strndup(p + arz_sym, eoa - (p + arz_sym));
		}
		/* and again, for rid= */
		for (const char *p = arg, *eoa;
		     (p = memmem(p, R(p), arg_rid, arz_rid)) != NULL &&
			     ((eoa = memchr(p, '&', R(p))) != NULL ||
			      (eoa = memchr(p, ' ', R(p))) != NULL);
		     p = eoa) {
			resize_rolf_objs(msg);
			msg->rolf_objs[msg->nrolf_objs++].rolf_id =
				strtoul(p + arz_rid, NULL, 10);
		}
#undef R
	}

	/* parse &select=... */
	{
		static const char arg_sel[] = "select=";
		static const size_t arz_sel = sizeof(arg_sel) - 1U;
		const char *p = arg;
		const char *eoa;

#define R(p)	(eol - (p))
		/* look for sym='s */
		if ((p = memmem(p, R(p), arg_sel, arz_sel)) != NULL &&
		    ((eoa = memchr(p, '&', R(p))) != NULL ||
		     (eoa = memchr(p, ' ', R(p))) != NULL)) {
			__parse_select(msg, p += arz_sel, eoa - p);
		}
#undef R
	}

	/* parse &filter=... */
	{
		static const char arg_flt[] = "filter=";
		static const size_t arz_flt = sizeof(arg_flt) - 1U;
		const char *p = arg;
		const char *eoa;

#define R(p)	(eol - (p))
		/* look for sym='s */
		if ((p = memmem(p, R(p), arg_flt, arz_flt)) != NULL &&
		    ((eoa = memchr(p, '&', R(p))) != NULL ||
		     (eoa = memchr(p, ' ', R(p))) != NULL)) {
			__parse_filter(msg, p += arz_flt, eoa - p);
		}
#undef R
	}

	/* find &ignore-case or &igncase */
	{
		static const char arg_ign[] = "ignore-case";
		static const char arg_igns[] = "igncase";
		static const size_t arz_ign = sizeof(arg_ign) - 1U;
		static const size_t arz_igns = sizeof(arg_igns) - 1U;
		const char *p;

#define R(p)	(eol - (p))
		if (((p = memmem(arg, R(arg), arg_ign, arz_ign)) != NULL &&
		     (p[arz_ign] == '&' || p[arz_ign] == ' ')) ||
		    ((p = memmem(arg, R(arg), arg_igns, arz_igns)) != NULL &&
		     (p[arz_igns] == '&' || p[arz_igns] == ' '))) {
			if (p > req && (p[-1] == '?' || p[-1] == '&')) {
				msg->igncase = 1;
			}
		}
#undef R
	}
	return 0;
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
	if (ctx != NULL) {
		*ctx = NULL;
	}

	if (UNLIKELY(bsz <= 4)) {
		return NULL;
	}

	/* just in case, we ask the if tree below to reset this to NULL
	 * if there's some form of mistake */
	res = __make_msg();

	if ((p = memmem(buf, bsz, "GET ", 4)) != NULL) {
		/* http request */
		rc = __parse_http(res, p, bsz - (p - buf));
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

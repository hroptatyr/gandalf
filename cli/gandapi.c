#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
/* mmap stuff */
#include <sys/mman.h>
#include <curl/curl.h>
/* our decls */
#include "gandapi.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */
#define ALGN16(_x)	__attribute__((aligned(16))) _x

#define countof(x)	(sizeof(x) / sizeof(*x))
#define assert(args...)
#define _paste(x, y)	x ## y
#define paste(x, y)	_paste(x, y)

#if !defined with
# define with(args...)							\
	for (args, *paste(__ep, __LINE__) = (void*)1;			\
	     paste(__ep, __LINE__); paste(__ep, __LINE__)= 0)
#endif	/* !with */


typedef struct __ctx_s *__ctx_t;

struct gand_ctx_s {
	/* curl context */
	CURL *curl_ctx;
	/* host name pointer */
	char *host;
	size_t hlen;
	/* callback and closure pointer */
	int(*_cb)(gand_res_t, void *cloptr);
	void *cloptr;
	/* timeout (in seconds now) */
	int32_t timeo;
};


/* parser goodies */
static int
xisspace(char tmp)
{
	switch (tmp) {
	case ' ':
	case '\t':
	case '\n':
		return 1;
	default:
		return 0;
	}
}

static size_t
xstrlncpy(char *restrict dst, size_t dsz, const char *src, size_t ssz)
{
	if (ssz > dsz) {
		if (UNLIKELY(dsz > 0U)) {
			return 0U;
		}
		ssz = dsz - 1U;
	}
	memcpy(dst, src, ssz);
	dst[ssz] = '\0';
	return ssz;
}

static char*
__p2c(const char *str)
{
	union {
		const char *c;
		char *p;
	} this = {.c = str};
	return this.p;
}

static idate_t
read_date(const char *str, char **restrict ptr)
{
#define C(x)	(x - '0')
	idate_t res = 0;
	const char *tmp;

	tmp = str;
	res = C(tmp[0]) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (*tmp == '\0' || xisspace(*tmp)) {
		if (ptr) {
			*ptr = __p2c(tmp);
		}
		return res;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (*tmp == '\0' || xisspace(*tmp)) {
		if (ptr) {
			*ptr = __p2c(tmp);
		}
		return res;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (*tmp == '\0' || xisspace(*tmp)) {
		/* date is fucked? */
		if (ptr) {
			*ptr = __p2c(tmp);
		}
		return 0;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2;

	if (ptr) {
		*ptr = __p2c(tmp);
	}
#undef C
	return res;
}

static size_t
_data_cb(void *buf, size_t chrz, size_t nchr, void *clo)
{
	const struct gand_ctx_s *g = clo;
	const char *const bp = buf;
	const size_t nrd = chrz * nchr;
	size_t i = 0U;

	/* unparse the buffer and call the callback */
	for (char *eol;
	     (eol = memchr(bp + i, '\n', nrd - i)) != NULL;
	     i = eol - bp + 1U) {
		struct gand_res_s r;
		const char *pp = bp + i;
		char *p;

		/* demark the eol */
		*eol = '\0';

		/* find end of symbol */
		if (UNLIKELY((p = strchr(pp, '\t')) == NULL)) {
			continue;
		}
		*p = '\0';
		r.symbol = pp;
		pp = p + 1U;

		/* p + 1 has the date now */
		if (UNLIKELY((p = strchr(pp, '\t')) == NULL)) {
			continue;
		}
		*p = '\0';
		r.date = read_date(pp, NULL);
		pp = p + 1U;

		/* p + 1 is the valflav */
		if (UNLIKELY((p = strchr(pp, '\t')) == NULL)) {
			continue;
		}
		*p = '\0';
		r.valflav = pp;
		pp = p + 1U;

		/* and finally the value
		 * stretches all the way to EOL which is \nul'd already */
		r.strval = pp;

		/* do the call */
		g->_cb(&r, g->cloptr);
	}
	return i;
}


/* exported functions */
#define PROT_MEM		(PROT_READ | PROT_WRITE)
#define MAP_MEM			(MAP_PRIVATE | MAP_ANONYMOUS)

gand_ctx_t
gand_open(const char *srv, int timeout)
{
	struct gand_ctx_s *res;
	CURL *ctx;

	/* don't allow NULL hosts */
	if (UNLIKELY(srv == NULL)) {
		return NULL;
	}

	/* try and get a curl easy handle */
	if (UNLIKELY((ctx = curl_easy_init()) == NULL)) {
		return NULL;
	}

	/* init structure */
	res = calloc(1, sizeof(*res));
	res->curl_ctx = ctx;

	with (size_t zrv = strlen(srv)) {
		res->host = strndup(srv, zrv);
		res->hlen = zrv;
	}

	res->timeo = timeout > 0 ? (timeout - 1) / 1000 + 1 : 0;
	return res;
}

void
gand_close(gand_ctx_t g)
{
	curl_easy_cleanup(g->curl_ctx);
	free(g->host);
	free(g);
	return;
}

const char*
gand_service(gand_ctx_t g)
{
	return g->host;
}

int
gand_get_series(
	gand_ctx_t g,
	const char *sym,
	int(*qcb)(gand_res_t, void *closure), void *closure)
{
	char buf[256U];

	/* set up our own context first */
	if (LIKELY(qcb != NULL)) {
		g->_cb = qcb;
		g->cloptr = closure;
	}

	/* reuse g's BUF to write the query string */
	with (size_t z) {
		if (UNLIKELY(!(z = xstrlncpy(
				       buf, sizeof(buf), g->host, g->hlen)))) {
			return -1;
		}
		if (buf[z - 1U] != '/') {
			buf[z++] = '/';
		}
		z += snprintf(
			buf + z, sizeof(buf) - z,
			"v0/series/%s?select=sym,d,vf,v&igncase", sym);
	}

	/* hand-over to libcurl */
	curl_easy_setopt(g->curl_ctx, CURLOPT_URL, buf);
	curl_easy_setopt(g->curl_ctx, CURLOPT_WRITEFUNCTION, _data_cb);
	curl_easy_setopt(g->curl_ctx, CURLOPT_WRITEDATA, g);
	curl_easy_setopt(g->curl_ctx, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(g->curl_ctx, CURLOPT_TIMEOUT, g->timeo);
	curl_easy_setopt(g->curl_ctx, CURLOPT_ENCODING, "gzip");
	if (curl_easy_perform(g->curl_ctx) != CURLE_OK) {
		return -1;
	}
	return 0;
}

/* gandapi.c ends here */

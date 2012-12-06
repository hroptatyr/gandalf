#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
/* socket stuff */
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
/* epoll stuff */
#include <sys/epoll.h>
#include <fcntl.h>
/* mmap stuff */
#include <sys/mman.h>
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

typedef struct __ctx_s *__ctx_t;

struct __ctx_s {
	/* epoll socket */
	int eps;
	/* gand sock */
	int gs;
	/* generic index */
	uint32_t idx;
	uint32_t nrd;
	/* buffer */
	char *buf;
	size_t bsz;
	/* timeout */
	int32_t timeo;
	/* number of events in the following flex array */
	int32_t nev;
	/* flex array of events, 8b-aligned */
	struct epoll_event ev[1];
};


/* gand details */
static __attribute__((unused)) void
setsock_nonblock(int sock)
{
	int opts;

	/* get former options */
	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		return;
	}
	opts |= O_NONBLOCK;
	(void)fcntl(sock, F_SETFL, opts);
	return;
}

static void
shut_sock(int s)
{
	shutdown(s, SHUT_RDWR);
	close(s);
	return;
}

#define STD_FLAGS	EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLRDHUP

static inline int
ep_prep(__ctx_t g, int s, int flags)
{
	struct epoll_event ev = {
		.events = flags,
		.data.fd = s
	};
	/* add S to the epoll descriptor EPFD */
	return epoll_ctl(g->eps, EPOLL_CTL_ADD, s, &ev);
}

static inline int
ep_fini(__ctx_t g, int s)
{
	/* remove S from the epoll descriptor EPFD */
	return epoll_ctl(g->eps, EPOLL_CTL_DEL, s, NULL);
}

static inline int
ep_wait(__ctx_t g, int timeout)
{
	/* wait and return */
	return epoll_wait(g->eps, g->ev, g->nev, timeout);
}

static int
init_sockaddr(struct sockaddr *sa, size_t *len, const char *host, uint16_t port)
{
	struct hostent *hi;
	struct sockaddr_in6 *n = (void*)sa;

	n->sin6_family = AF_INET6;
	n->sin6_port = htons(port);
	if ((hi = gethostbyname2(host, AF_INET6)) == NULL) {
		return -1;
	}
	memcpy(&n->sin6_addr, hi->h_addr, sizeof(n->sin6_addr));
	*len = sizeof(*n);
	return 0;
}


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


/* exported functions */
#define PROT_MEM		(PROT_READ | PROT_WRITE)
#define MAP_MEM			(MAP_PRIVATE | MAP_ANONYMOUS)

gand_ctx_t
gand_open(const char *srv, int timeout)
{
	volatile int ns;
	struct sockaddr_storage sa = {0};
	size_t sa_len = sizeof(sa);
	int fam;
	struct __ctx_s *res = calloc(1, sizeof(*res));
	/* to parse the service string */
	char *p;

	if ((p = strrchr(srv, ':')) == NULL) {
		/* must be a unix socket then */
		fam = PF_LOCAL;
		;
	} else {
		uint16_t port = strtoul(p + 1, NULL, 10) ?: 8624;
		char host[p - srv + 1];

		fam = PF_INET6;
		if (srv[0] == '[' && p[-1] == ']') {
			memcpy(host, srv + 1, p - srv - 2);
			host[p - srv - 2] = '\0';
		} else {
			memcpy(host, srv, p - srv);
			host[p - srv] = '\0';
		}
		if (init_sockaddr((void*)&sa, &sa_len, host, port) < 0) {
			return res;
		}
	}

	if ((ns = socket(fam, SOCK_STREAM, 0)) < 0) {
		return res;
	} else if (connect(ns, (void*)&sa, sa_len) < 0) {
		return res;
	}

	/* init structure */
	res->eps = epoll_create1(0);
	res->gs = ns;
	res->nev = countof(res->ev);

	res->bsz = 16 * 4096;
	res->buf = mmap(NULL, res->bsz, PROT_MEM, MAP_MEM, 0, 0);

	res->timeo = timeout;

	/* set up epoll */
	setsock_nonblock(res->eps);
	setsock_nonblock(res->gs);
	return res;
}

void
gand_close(gand_ctx_t ug)
{
	struct __ctx_s *g = ug;
	/* stop waiting for events */
	ep_fini(g, g->gs);
	shut_sock(g->gs);
	shut_sock(g->eps);
	munmap(g->buf, g->bsz);
	free(g);
	return;
}

int
gand_send(gand_ctx_t ug, const char *qry, size_t qsz)
{
	__ctx_t g = ug;
	int nfds __attribute__((unused)) = 1;

	/* setup event waiter */
	ep_prep(g, g->gs, STD_FLAGS | EPOLLET | EPOLLIN | EPOLLOUT);

	do {
		typeof(g->ev->events) ev = g->ev[0].events;
		int fd = g->ev[0].data.fd;

		/* we've only asked for one, so it would be peculiar */
		assert(nfds == 1);

		if (ev & EPOLLIN) {
			/* must be garbage, wipe it */
			ssize_t nrd;
			size_t tot = 0;
			while ((nrd = read(fd, g->buf, g->bsz)) > 0) {
				tot += nrd;
			}

		} else if (LIKELY(ev & EPOLLOUT)) {
			ssize_t nwr;
			size_t tot = 0;

			while (tot < qsz &&
			       (nwr = write(fd, qry + tot, qsz - tot)) > 0) {
				tot += nwr;
			}
			if (tot >= qsz) {
				g->idx = g->nrd = 0;
				return 0;
			}

		} else if (ev == 0) {
			/* state unknown, better poll */

		} else {
			break;
		}
	} while ((nfds = ep_wait(g, g->timeo)) > 0);
	return -1;
}

ssize_t
gand_recv(gand_ctx_t ug, const char **buf)
{
/* coroutine */
	__ctx_t g = ug;
	int nfds __attribute__((unused)) = 1;

find:
	if (g->idx < g->nrd) {
		char *p = memchr(g->buf + g->idx, '\n', g->nrd - g->idx);
		if (p) {
			size_t res = p - (g->buf + g->idx);
			*p = '\0';
			*buf = g->buf + g->idx;
			g->idx += res + 1;
			return res;
		}
		/* memmove what we've got */
		memmove(g->buf, g->buf + g->idx, g->nrd - g->idx);
		g->nrd -= g->idx;
		g->idx = 0;

	} else if (g->idx > 0) {
		/* message has been finished prematurely, we assume thats it
		 * ATTENTION this might go wrong if the total message size
		 * is divisible by the buffer size, in which case we wait
		 * for another SRV_TIMEOUT millis, a well */
		g->idx = g->nrd = 0;
		g->timeo = 10;
	}

	do {
		typeof(g->ev->events) ev;
		int fd;

		/* we've only asked for one, so it would be peculiar */
		assert(nfds == 1);

		ev = g->ev[0].events;
		fd = g->ev[0].data.fd;

		if (LIKELY(ev & EPOLLIN)) {
			ssize_t nrd;

			if ((nrd = read(
				     fd,
				     g->buf + g->nrd, g->bsz - g->nrd)) > 0) {
				g->nrd += nrd;
				goto find;
			} else if (nrd == 0) {
				/* ah, eo-msg */
				*buf = NULL;
				return -1;
			}
		} else if (ev & EPOLLOUT) {
			/* do nothing */
			;
		} else {
			return -1;
		}
	} while ((nfds = ep_wait(g, g->timeo)) > 0);
	/* rinse */
	return -1;
}

int
gand_get_series(
	gand_ctx_t ug,
	const char *sym, char *const valflav[], size_t nvalflav,
	int(*qcb)(gand_res_t, void *closure), void *closure)
{
	static const char dflt[] = "fix/stl/close/unknown";
	static const char rhdr[] = " HTTP/1.1\r\n\
Connection: keep-alive\r\n\
User-Agent: gandapi\r\n\
\r\n";
	struct __ctx_s *g = ug;
	/* we just use g's buffer to offload our query */
	size_t gqlen;
	/* result buffer */
	const char *rb;

	gqlen = snprintf(
		g->buf, g->bsz,
		"GET /series?sym=%s&select=sym,d,vf,v&igncase&filter=", sym);

	if (valflav == NULL || nvalflav == 0) {
		memcpy(g->buf + gqlen, dflt, countof(dflt));
		gqlen += countof(dflt) - 1;
	} else {
		for (size_t i = 0; i < nvalflav; i++) {
			const char *vf = valflav[i];
			size_t vflen = strlen(vf);

			if (UNLIKELY(gqlen + vflen >= g->bsz)) {
				break;
			}
			memcpy(g->buf + gqlen, vf, vflen);
			g->buf[gqlen += vflen] = ',';
			gqlen++;
		}
		--gqlen;
	}

	/* copy the rest of the header */
	memcpy(g->buf + gqlen, rhdr, sizeof(rhdr) - 1);
	gqlen += sizeof(rhdr) - 1;

	/* query is ready now */
	if (gand_send(g, g->buf, gqlen) < 0) {
		return -1;
	}

	while (gand_recv(g, &rb) > 0) {
		struct gand_res_s res;
		char *p;

		/* find end of symbol */
		if (UNLIKELY((p = strchr(rb, '\t')) == NULL)) {
			continue;
		}
		*p = '\0';
		res.symbol = rb;

		/* p + 1 has the date now */
		if (UNLIKELY((p = strchr((rb = p + 1), '\t')) == NULL)) {
			continue;
		}
		*p = '\0';
		res.date = read_date(rb, NULL);

		/* p + 1 is the valflav */
		if (UNLIKELY((p = strchr((rb = p + 1), '\t')) == NULL)) {
			continue;
		}
		*p = '\0';
		res.valflav = rb;

		/* and finally the value */
		res.value = strtod(p + 1, NULL);

		/* do the call */
		qcb(&res, closure);
	}
	return 0;
}

/* gandapi.c ends here */

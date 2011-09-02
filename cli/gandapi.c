#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* socket stuff */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
/* epoll stuff */
#include <sys/epoll.h>
#include <fcntl.h>
/* our decls */
#include "gandapi.h"

#define countof(x)	(sizeof(x) / sizeof(*x))
#define assert(args...)
#define LIKELY(x)	(x)

typedef struct __ctx_s *__ctx_t;

struct __ctx_s {
	/* epoll socket */
	int eps;
	/* gand sock */
	int gs;
	/* generic index */
	uint32_t idx;
	uint32_t nrd;
	/* number of events in the following flex array */
	size_t nev;
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

int
gand_send(gand_ctx_t ug, const char *qry, size_t qsz, int timeout)
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
			while ((nrd = read(fd, gbuf, sizeof(gbuf))) > 0) {
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
	} while ((nfds = ep_wait(g, timeout)) > 0);
	return -1;
}

ssize_t
gand_recv(gand_ctx_t ug, const char **buf, int timeout)
{
/* coroutine */
	__ctx_t g = ug;
	int nfds __attribute__((unused)) = 1;

find:
	if (g->idx < g->nrd) {
		char *p = memchr(gbuf + g->idx, '\n', g->nrd - g->idx);
		if (p) {
			size_t res = p - (gbuf + g->idx);
			*p = '\0';
			*buf = gbuf + g->idx;
			g->idx += res + 1;
			return res;
		}
		/* memmove what we've got */
		memmove(gbuf, gbuf + g->idx, g->nrd - g->idx);
		g->idx = g->nrd - g->idx;

	} else if (g->nrd > 0 && g->nrd < countof(gbuf)) {
		/* message has been finished prematurely, we assume thats it
		 * ATTENTION this might go wrong if the total message size
		 * is divisible by the buffer size, in which case we wait
		 * for another SRV_TIMEOUT millis, a well */
		*buf = NULL;
		return -1;
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

			if ((nrd = read(fd, gbuf, sizeof(gbuf) - g->idx)) > 0) {
				g->nrd = nrd;
				goto find;
			} else if (nrd == 0) {
				/* ah, eo-msg */
				*buf = NULL;
				return -1;
			}

		} else if (ev & EPOLLOUT) {
			/* uh oh */
			;
		} else {
			break;
		}
	} while ((nfds = ep_wait(g, timeout)) > 0);
	/* rinse */
	return 0;
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


/* exported functions */
gand_ctx_t
gand_open(const char *srv)
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

	/* set up epoll */
	setsock_nonblock(res->eps);
	setsock_nonblock(res->gs);
	return res;
}

void
gand_close(gand_ctx_t g)
{
	struct __ctx_s *__g = g;
	/* stop waiting for events */
	ep_fini(__g, __g->gs);
	shut_sock(__g->eps);
	free(g);
	return;
}

int
gand_query(
	gand_ctx_t g,
	const char *qry, size_t qsz, int timeout,
	int(*qcb)(gand_res_t, void *closure), void *closure)
{
	struct __ctx_s *__g = g;

	return 0;
}


/* parser */
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

/* gandapi.c ends here */

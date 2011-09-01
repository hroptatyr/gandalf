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
/* matlab stuff */
#include "mex.h"

/**
 * [d, p] = gand_get_series(service, symbol, valflav, ...)
 *          reads a gandalf time series
 *
 * Input:
 * symbol
 * optional: valflav cell array
 *
 * Output:
 * date, price time series, one column per valflav
 *
 **/

#define countof(x)	(sizeof(x) / sizeof(*x))
#define assert(args...)
#define LIKELY(x)	(x)

typedef struct gand_ctx_s *gand_ctx_t;
typedef uint32_t idate_t;

struct gand_ctx_s {
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

/* this would be ute really */
struct pvv_s {
	uint32_t cid;
	uint32_t t;
	double v;
};

/* grrrr */
extern void mexFunction(int, mxArray*[], int, const mxArray*[]);


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
ep_prep(gand_ctx_t g, int s, int flags)
{
	struct epoll_event ev = {
		.events = flags,
		.data.fd = s
	};
	/* add S to the epoll descriptor EPFD */
	return epoll_ctl(g->eps, EPOLL_CTL_ADD, s, &ev);
}

static inline int
ep_fini(gand_ctx_t g, int s)
{
	/* remove S from the epoll descriptor EPFD */
	return epoll_ctl(g->eps, EPOLL_CTL_DEL, s, NULL);
}

static inline int
ep_wait(gand_ctx_t g, int timeout)
{
	/* wait and return */
	return epoll_wait(g->eps, g->ev, g->nev, timeout);
}

static char gbuf[4096];

static int
gand_send(gand_ctx_t g, const char *qry, size_t qsz)
{
#define SRV_TIMEOUT	(2000/*millis*/)
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
	} while ((nfds = ep_wait(g, SRV_TIMEOUT)) > 0);
	return -1;
}

static size_t
gand_recv(gand_ctx_t g, const char **buf)
{
/* coroutine */
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
		return 0;
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
				return 0;
			}

		} else if (ev & EPOLLOUT) {
			/* uh oh */
			;
		} else {
			break;
		}
	} while ((nfds = ep_wait(g, SRV_TIMEOUT)) > 0);
	/* rinse */
	return 0;
}

static int
gand_fini(gand_ctx_t g)
{
	/* stop waiting for events */
	ep_fini(g, g->gs);
	return 0;
}

static void
init_sockaddr(struct sockaddr *sa, size_t *len, const char *host, uint16_t port)
{
	struct hostent *hi;
	struct sockaddr_in6 *n = (void*)sa;

	n->sin6_family = AF_INET6;
	n->sin6_port = htons(port);
	if ((hi = gethostbyname2(host, AF_INET6)) == NULL) {
		return;
	}
	memcpy(&n->sin6_addr, hi->h_addr, sizeof(n->sin6_addr));
	*len = sizeof(*n);
	return;
}

static void
be_gand_open(gand_ctx_t g, const char *host, uint16_t port)
{
	volatile int ns;
	struct sockaddr_storage sa = {0};
	size_t sa_len = sizeof(sa);
	int fam = port ? PF_INET6 : PF_LOCAL;

	if ((ns = socket(fam, SOCK_STREAM, 0)) < 0) {
		return;
	}

	init_sockaddr((void*)&sa, &sa_len, host, port);
	if (connect(ns, (void*)&sa, sa_len) < 0) {
		return;
	}

	/* init structure */
	memset(g, 0, sizeof(*g));
	g->eps = epoll_create1(0);
	g->gs = ns;
	g->nev = countof(g->ev);

	/* set up epoll */
	setsock_nonblock(g->eps);
	setsock_nonblock(g->gs);
	return;
}

static void
be_gand_close(gand_ctx_t g)
{
	/* stop waiting for events */
	gand_fini(g);
	shut_sock(g->eps);
	return;
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

static int
find_valflav(const char *vf, const mxArray *prhs[], size_t nrhs)
{
	if (nrhs == 2) {
		return 0;
	}
	/* else */
	for (size_t i = 2; i < nrhs; i++) {
		const char *p = mxArrayToString(prhs[i]);
		if (strcmp(p, vf) == 0) {
			return i - 2;
		}
	}
	return -1;
}


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	struct gand_ctx_s gctx[1];
	const char *srv;
	const char *sym;
	char gandq[256];
	size_t gqlen;
	/* state for retrieval */
	struct __recv_st_s {
		idate_t ldat;
		uint32_t lidx;
	} recv_st = {
		.ldat = 0,
		.lidx = 0,
	};
	int ncols;
	/* gandalf iterator */
	const char *rb;

	if (nrhs == 0 ||
	    (srv = mxArrayToString(prhs[0])) == NULL) {
		mexErrMsgTxt("service string not defined\n");
	} else if (nrhs == 1 ||
		   (sym = mxArrayToString(prhs[1])) == NULL) {
		mexErrMsgTxt("symbol not given\n");
	}

	gqlen = snprintf(
		gandq, countof(gandq),
		"get_series \"%s\" --select d,vf,v -i --filter ", sym);

	if (nrhs == 2) {
		/* no valflavs given, proceed with defaults */
		static const char dflt[] = "fix/stl/close/unknown";
		
		memcpy(gandq + gqlen, dflt, countof(dflt));
		gqlen += countof(dflt);
	} else {
		for (size_t i = 2; i < nrhs; i++) {
			const char *const vf = mxArrayToString(prhs[i]);
			size_t vflen = strlen(vf);

			if (gqlen + vflen >= countof(gandq)) {
				break;
			}
			memcpy(gandq + gqlen, vf, vflen);
			gandq[gqlen += vflen] = '+';
			gqlen++;
		}
		--gqlen;
	}

	if (nlhs == 0) {
		return;
	} else {
		/* parse the service string */
		char *p;

		if ((p = strchr(srv, ':')) == NULL) {
			/* bugger, no port, prob a unix socket */
			be_gand_open(gctx, srv, 0);
		} else {
			char host[64];
			uint16_t port; 

			memcpy(host, srv, p - srv);
			host[p - srv] = '\0';
			port = strtoul(p + 1, NULL, 10);
			be_gand_open(gctx, host, port);
		}
	}

	if (gand_send(gctx, gandq, gqlen) < 0) {
		goto out;
	}

	/* get started with the date vector */
	plhs[0] = mxCreateDoubleMatrix(1024, 1, mxREAL);
	if (nlhs > 1) {
		ncols = nrhs == 2 ? 1 : nrhs - 2;
		plhs[1] = mxCreateDoubleMatrix(1024, ncols, mxREAL);
	}

	while (gand_recv(gctx, &rb) > 0) {
		char *more;
		char *eovf;
		idate_t this_dat = read_date(rb, &more);
		uint32_t this_vf;
		double this_v;

		if (this_dat > recv_st.ldat) {
			if (++recv_st.lidx % 1024 == 0) {
				/* resize */
				mxSetM(plhs[0], recv_st.lidx + 1024);
				if (nlhs > 1) {
					mxSetM(plhs[1], recv_st.lidx + 1024);
				}
			}
			recv_st.ldat = this_dat;
			mxGetPr(plhs[0])[recv_st.lidx - 1] = this_dat;
		}
		if (nlhs == 1) {
			continue;
		}
		/* otherwise also fill the second matrix */
		/* (more + 1) points to the valflav now */
		if ((eovf = strchr(more + 1, '\t')) == NULL) {
			/* line buggered */
			continue;
		}
		*eovf = '\0';
		this_vf = find_valflav(more + 1, prhs, nrhs);
		this_v = strtod(eovf + 1, NULL);

		/* now finally read the value */
		mxGetPr(plhs[1])[(recv_st.lidx - 1) * mxGetN(plhs[1]) + this_vf] = this_v;
	}

	/* now reset the matrices to their true dimensions */
	mxSetM(plhs[0], recv_st.lidx);
	if (nlhs > 1) {
		mxSetM(plhs[1], recv_st.lidx);
	}

out:
	/* and fuck off */
	be_gand_close(gctx);
	return;
}

/* gand_get_series.c ends here */

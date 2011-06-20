/*** just to focus on the essential stuff in the dso-gand module */
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <errno.h>
/* we just need the headers hereof and hope that unserding used the same ones */
#include <ev.h>
#include "nifty.h"
#include "con6ity.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:2259)
# pragma warning (disable:177)
#endif	/* __INTEL_COMPILER */

#define C10Y_PRE	"mod/gand/con6ity"

typedef	enum {
	WBUF_FL_NIL = 0,
	/* autoset when free()'d or munmap()ped */
	WBUF_FL_FINISHED = 1,
	/* set when buffer should be freed after sending */
	WBUF_FL_FREE = 2,
	/* set when buffer should be munmapped after sending */
	WBUF_FL_MUNMAP = 3,
	/* keep the struct alive, but prescind from further notifs */
	WBUF_FL_KEEP = 4,
} conn_flag_t;

struct __wbuf_s {
	ev_io io[1];
	union {
		char *buf;
		const char *cbuf;
	};
	size_t len;
	size_t nwr;
	unsigned int flags;
	int(*notify_cb)(gand_conn_t);
	void *neigh;
};


/* services for includers that need not know about libev */
#define FD_MAP_TYPE	ev_io*

DEFUN int __attribute__((unused))
get_fd(gand_conn_t ctx)
{
	FD_MAP_TYPE io = ctx;
	return io->fd;
}

DEFUN void* __attribute__((unused))
get_fd_data(gand_conn_t ctx)
{
	FD_MAP_TYPE io = ctx;
	return io->data;
}

DEFUN void __attribute__((unused))
put_fd_data(gand_conn_t ctx, void *data)
{
	FD_MAP_TYPE io = ctx;
	io->data = data;
	return;
}


/* connection mumbo-jumbo */
size_t nwio = 0;
static ev_io __wio[2];
static void *gloop = NULL;

static void
__shut_sock(int s)
{
	shutdown(s, SHUT_RDWR);
	close(s);
	return;
}

static void
clo_wio(EV_P_ ev_io *w)
{
	fsync(w->fd);
	ev_io_stop(EV_A_ w);
	__shut_sock(w->fd);
	xfree(w);
	return;
}

static inline int
getsockopt_int(int s, int level, int optname)
{
	int res[1];
	socklen_t rsz = sizeof(*res);
	if (getsockopt(s, level, optname, res, &rsz) >= 0) {
		return *res;
	}
	return -1;
}

static inline int
setsockopt_int(int s, int level, int optname, int value)
{
	return setsockopt(s, level, optname, &value, sizeof(value));
}

/**
 * Mark address behind socket S as reusable. */
static inline int
setsock_reuseaddr(int s)
{
#if defined SO_REUSEADDR
	return setsockopt_int(s, SOL_SOCKET, SO_REUSEADDR, 1);
#else  /* !SO_REUSEADDR */
	return 0;
#endif	/* SO_REUSEADDR */
}

/* probably only available on BSD */
static inline int
setsock_reuseport(int __attribute__((unused)) s)
{
#if defined SO_REUSEPORT
	return setsockopt_int(s, SOL_SOCKET, SO_REUSEPORT, 1);
#else  /* !SO_REUSEPORT */
	return 0;
#endif	/* SO_REUSEPORT */
}

/* we could take args like listen address and port number */
DEFUN int
conn_listener_net(uint16_t port)
{
#if defined IPPROTO_IPV6
	static struct sockaddr_in6 __sa6 = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT
	};
	volatile int s;

	/* non-constant slots of __sa6 */
	__sa6.sin6_port = htons(port);

	if (LIKELY((s = socket(PF_INET6, SOCK_STREAM, 0)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		GAND_DEBUG(C10Y_PRE ": socket() failed ... I'm clueless now\n");
		return s;
	}

#if defined IPV6_V6ONLY
	setsockopt_int(s, IPPROTO_IPV6, IPV6_V6ONLY, 0);
#endif	/* IPV6_V6ONLY */
#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, 1);
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_DONTFRAG, 1);
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, 1);
#endif
	setsock_reuseaddr(s);
	setsock_reuseport(s);

	/* we used to retry upon failure, but who cares */
	if (bind(s, (struct sockaddr*)&__sa6, sizeof(__sa6)) < 0 ||
	    listen(s, 2) < 0) {
		GAND_DEBUG(C10Y_PRE ": bind() failed, errno %d\n", errno);
		close(s);
		return -1;
	}
	return s;

#else  /* !IPPROTO_IPV6 */
	return -1;
#endif	/* IPPROTO_IPV6 */
}

DEFUN int
conn_listener_uds(const char *sock_path)
{
	static struct sockaddr_un __s = {
		.sun_family = AF_LOCAL,
	};
	volatile int s;
	size_t sz;

	if (LIKELY((s = socket(PF_LOCAL, SOCK_DGRAM, 0)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		GAND_DEBUG(C10Y_PRE ": socket() failed ... I'm clueless now\n");
		return s;
	}

	/* bind a name now */
	strncpy(__s.sun_path, sock_path, sizeof(__s.sun_path));
	__s.sun_path[sizeof(__s.sun_path) - 1] = '\0';

	/* The size of the address is
	   the offset of the start of the filename,
	   plus its length,
	   plus one for the terminating null byte.
	   Alternatively you can just do:
	   size = SUN_LEN (&name); */
	sz = offsetof(struct sockaddr_un, sun_path) + strlen(__s.sun_path) + 1;

	/* just unlink the socket */
	unlink(sock_path);
	/* we used to retry upon failure, but who cares */
	if (bind(s, (struct sockaddr*)&__s, sz) < 0) {
		GAND_DEBUG(C10Y_PRE ": bind() failed: %s\n", strerror(errno));
		close(s);
		return -1;
	}
	return s;
}


/* weak functions first */
#if !defined HAVE_handle_data
DEFUN_W int
handle_data(gand_conn_t UNUSED(c), char *UNUSED(msg), size_t UNUSED(msglen))
{
	return 0;
}
#endif	/* !HAVE_handle_data */

#if !defined HAVE_handle_close
DEFUN_W void
handle_close(gand_conn_t UNUSED(c))
{
	return;
}
#endif	/* !HAVE_handle_close */

static void
writ_cb(EV_P_ ev_io *e, int UNUSED(re))
{
	int fd = e->fd;
	struct __wbuf_s *wb = (void*)e;
	size_t len = wb->len - wb->nwr;
	ssize_t nwr;

	if ((nwr = write(fd, wb->cbuf + wb->nwr, len)) < 0) {
		goto clo;
	}
	GAND_DEBUG(C10Y_PRE ": wrote %zd to %d\n", nwr, fd);
	if ((wb->nwr += nwr) < wb->len) {
		return;
	}
clo:
	GAND_DEBUG(C10Y_PRE ": %d not needed for write, cleaning up\n", fd);
	/* unsubscribe interest */
	ev_io_stop(EV_A_ e);

	if (wb->notify_cb) {
		/* call the user's idea of what has to be done now */
		wb->notify_cb(wb);
	}
	if ((wb->flags & (WBUF_FL_FREE | WBUF_FL_MUNMAP)) ==
	    WBUF_FL_MUNMAP) {
		munmap(wb->buf, wb->len);
		wb->flags &= ~(WBUF_FL_FREE | WBUF_FL_MUNMAP);
		wb->flags |= (WBUF_FL_FINISHED);
	} else if ((wb->flags & (WBUF_FL_FREE | WBUF_FL_MUNMAP)) ==
		   WBUF_FL_FREE) {
		free(wb->buf);
		wb->flags &= ~(WBUF_FL_FREE | WBUF_FL_MUNMAP);
		wb->flags |= (WBUF_FL_FINISHED);
	}
	if (wb->flags & WBUF_FL_KEEP) {
		free(wb);
	}
	/* remove ourselves from our neighbour's slot */
	put_fd_data(wb->neigh, NULL);
	return;
}

static void
data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	char buf[4096];
	ssize_t nrd;
	void *ctx;

	if ((nrd = read(w->fd, buf, sizeof(buf))) <= 0) {
		goto clo;
	}
	GAND_DEBUG(C10Y_PRE ": new data in sock %d\n", w->fd);
	if (LIKELY(nrd < sizeof(buf))) {
		/* seeing as we get around to it */
		buf[nrd] = '\0';
	}
	if (handle_data(w, buf, nrd) < 0) {
		goto clo;
	}
	return;
clo:
	if ((ctx = get_fd_data(w)) != NULL) {
		struct __wbuf_s *wb = ctx;
		GAND_DEBUG(C10Y_PRE ": unfinished business on %p\n", ctx);
		;
		return;
	}
	GAND_DEBUG(C10Y_PRE ": %zd data, closing socket %d\n", nrd, w->fd);
	handle_close(w);
	clo_wio(EV_A_ w);
	return;
}

static void
inco_cb(EV_P_ ev_io *w, int UNUSED(re))
{
/* we're tcp so we've got to accept() the bugger, don't forget :) */
	volatile int ns;
	ev_io *aw;
	struct sockaddr_storage sa;
	socklen_t sa_size = sizeof(sa);

	GAND_DEBUG(C10Y_PRE ": they got back to us...");
	if ((ns = accept(w->fd, (struct sockaddr*)&sa, &sa_size)) < 0) {
		GAND_DBGCONT("accept() failed\n");
		return;
	}

        /* make an io watcher and watch the accepted socket */
	aw = xnew(ev_io);
        ev_io_init(aw, data_cb, ns, EV_READ);
	aw->data = NULL;
        ev_io_start(EV_A_ aw);
	GAND_DBGCONT("success, new sock %d\n", ns);
	return;
}

static void
clo_evsock(EV_P_ int UNUSED(type), void *w)
{
	ev_io *wp = w;

        /* deinitialise the io watcher */
        ev_io_stop(EV_A_ wp);
	/* properly shut the socket */
	__shut_sock(wp->fd);
	return;
}


DEFUN void
init_conn_watchers(void *loop, int s)
{
	struct ev_io *wio = __wio + nwio++;

	if (s < 0) {
		return;
	}

        /* initialise an io watcher, then start it */
        ev_io_init(wio, inco_cb, s, EV_READ);
        ev_io_start(EV_A_ wio);
	/* last loop wins */
	gloop = loop;
	return;
}

DEFUN void
deinit_conn_watchers(void *UNUSED(loop))
{
#if defined EV_WALK_ENABLE && EV_WALK_ENABLE
	/* properly close all sockets */
	ev_walk(EV_A_ EV_IO, clo_evsock);
#else  /* !EV_WALK_ENABLE */
	for (size_t i = 0; i < nwio; i++) {
		clo_evsock(EV_A_ EV_IO, __wio + i);
	}
#endif	/* EV_WALK_ENABLE */
	return;
}


/* helpers for careless writing */
DEFUN_W gand_conn_t
write_soon(gand_conn_t conn, const char *buf, size_t len, int(*cb)(gand_conn_t))
{
	struct __wbuf_s *wb;

	if (buf == NULL || len == 0) {
		return NULL;
	}
	/* otherwise the user isn't so much a prick as we thought*/
	wb = xnew(*wb);
	/* fill in */
	wb->cbuf = buf;
	wb->len = len;
	wb->nwr = 0UL;
	wb->flags = WBUF_FL_NIL;
	wb->notify_cb = cb;
	wb->neigh = conn;

	/* finally we pretend interest in this socket */
        ev_io_init(wb->io, writ_cb, ((FD_MAP_TYPE)conn)->fd, EV_WRITE);
        ev_io_start(gloop, wb->io);
	return wb;
}

DEFUN_W void
set_conn_flag_free(gand_conn_t conn)
{
	struct __wbuf_s *wb = conn;
	wb->flags |= WBUF_FL_FREE;
	return;
}

DEFUN_W void
set_conn_flag_keep(gand_conn_t conn)
{
	struct __wbuf_s *wb = conn;
	wb->flags |= WBUF_FL_KEEP;
	return;
}

DEFUN_W void
set_conn_flag_munmap(gand_conn_t conn)
{
	struct __wbuf_s *wb = conn;
	wb->flags |= WBUF_FL_MUNMAP;
	return;
}

/* con6ity.c ends here */

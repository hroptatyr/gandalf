/*** httpd.h -- our own take on serving via http
 *
 * Copyright (C) 2010-2014 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of gandalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
/***
 * This subsystem solely exists because we weren't happy with David
 * Moreno's libonion.  We also tried servlet daemons, G-WAN and nxweb:
 * G-WAN is rather unstable, nxweb needs to many fixes to be used as is. */
#if !defined INCLUDED_httpd_h_
#define INCLUDED_httpd_h_

typedef struct gand_httpd_s *gand_httpd_t;

typedef enum {
	VERB_UNSUPP = 0U,
	VERB_GET,
	VERB_HEAD,
	VERB_POST,
	VERB_PUT,
	VERB_DELETE,
	VERB_CONNECT,
	VERB_OPTIONS,
	VERB_TRACE,
	NVERBS
} gand_httpd_verb_t;

typedef struct {
	const char *str;
	size_t len;
} gand_word_t;

/* just an ordinary pointer but managed by ourselves. */
typedef struct gand_gbuf_s *gand_gbuf_t;

/* just wrap the OS's file descriptors */
typedef int gand_sock_t;

/* response data type */
typedef enum gand_dtyp_e gand_dtyp_t;

typedef struct {
	enum gand_dtyp_e {
		/* send a zero-length response
		 * this can be used to transmit http error codes */
		DTYP_NONE,
		/* send contents of file FILE */
		DTYP_FILE,
		/* like DTYP_FILE but remove the file after transmission */
		DTYP_TMPF,
		/* send contents of file descriptor, close after use */
		DTYP_SOCK,

		/* send contents of buffer DATA,
		 * this should be static or otherwise managed because
		 * there will be no dtor calls or other forms of notification
		 * that the buffer is no longer used
		 * for dynamic one-off buffers use gand_gbuf_t objects */
		DTYP_DATA,
		/* send contents of buffer GBUF and free it afterwards */
		DTYP_GBUF,
	} dtyp;
	union {
		const void *ptr;
		const char *file;
		gand_sock_t sock;
		const char *data;
		gand_gbuf_t gbuf;
	};
} gand_res_data_t;

typedef struct {
	gand_httpd_verb_t verb;
	gand_word_t hdr;
	gand_word_t data;
	const char *host;
	const char *path;
	const char *query;
} gand_httpd_req_t;

typedef struct {
	/** http response code */
	unsigned int rc;
	/** content type */
	const char *ctyp;
	/** content length
	 * if unknown use CLEN_UNKNOWN and it will be
	 * calculated in the case of DTYP_FILE, or DTYP_GBUF. */
	size_t clen;
#define CLEN_UNKNOWN	(0U)
	/** response data */
	gand_res_data_t rd;
} gand_httpd_res_t;

/* parameter struct for make_gand_httpd() */
typedef struct {
	/** port to listen on */
	short unsigned int port;
	/** timeout in useconds, 0 to disable */
	unsigned int timeout;
	/** directory from which to serve files */
	const char *www_dir;
	/** name of the server and version */
	const char *server;
	/** routine to respond to a request. */
	gand_httpd_res_t(*workf)(gand_httpd_req_t);
} gand_httpd_param_t;

/* public part of gand_httpd_s */
struct gand_httpd_s {
	const gand_httpd_param_t param;
};


/**
 * Instantiate a new http daemon according to params.
 * Return NULL on failure. */
extern gand_httpd_t make_gand_httpd(const gand_httpd_param_t);

/**
 * Free resources associated with the httpd object. */
extern void free_gand_httpd(gand_httpd_t);

/**
 * Main loop. */
extern void gand_httpd_run(gand_httpd_t);

/**
 * Helper getter for gand requests. */
extern gand_word_t gand_req_get_xhdr(gand_httpd_req_t req, const char *hdr);

/**
 * Helper getter for gand requests. */
extern gand_word_t gand_req_get_xqry(gand_httpd_req_t req, const char *fld);


/* buffer goodness */
/**
 * Obtain a buffer `socket' data can be written to.
 * ESTIMATE may specify a rough estimate on the size of the buffer, just
 * to avoid frequent resizings.  This is in no way binding, buffers can
 * be underfilled or overfilled. */
extern gand_gbuf_t make_gand_gbuf(size_t estimate);

/**
 * Return the gbuf object to the buffer pool.
 * This will give up all the bytes written to it. */
extern void free_gand_gbuf(gand_gbuf_t);

/**
 * Write (i.e. copy) Z bytes from P to the internal buffer GB. */
extern ssize_t gand_gbuf_write(gand_gbuf_t, const void *p, size_t z);

#endif	/* INCLUDED_httpd_h_ */

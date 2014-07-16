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

typedef struct {
	short unsigned int port;
	int timeout;
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

#endif	/* INCLUDED_httpd_h_ */

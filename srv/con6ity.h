/*** con6ity.h -- abstract from networking mumbo jumbo
 *
 * Copyright (C) 2011 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of the army of unserding daemons.
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

#if !defined INCLUDED_con6ity_h_
#define INCLUDED_con6ity_h_

#include <sys/types.h>

#if !defined DECLF
# define DECLF		extern
#endif	/* !DECLF */
#if !defined DECLF_W
# define DECLF_W	extern __attribute__((weak))
#endif	/* !DECLF */
#if !defined DEFUN
# define DEFUN
#endif	/* !DEFUN */
#if !defined DEFUN_W
# define DEFUN_W	__attribute__((weak))
#endif	/* !DEFUN */

typedef void *gand_conn_t;

DECLF int get_fd(gand_conn_t ctx);
DECLF void *get_fd_data(gand_conn_t ctx);
DECLF void put_fd_data(gand_conn_t ctx, void *data);

DECLF int conn_listener_net(uint16_t port);
DECLF int conn_listener_uds(const char *sock_path);
DECLF void init_conn_watchers(void *loop, int s);
DECLF void deinit_conn_watchers(void *loop);


/* stuff the user should/must overwrite, to be replaced with callbacks */
DECLF_W int handle_data(gand_conn_t, char *msg, size_t msglen);
DECLF_W void handle_close(gand_conn_t);

/* helper functions for as long as there is no edge-triggered writer
 * CB is called when the buffer has been written completely or there
 * was an error. */
DECLF_W gand_conn_t
write_soon(gand_conn_t, const char *buf, size_t len, int(*cb)(gand_conn_t));

DECLF_W void set_conn_flag_free(gand_conn_t conn);
DECLF_W void set_conn_flag_keep(gand_conn_t conn);
DECLF_W void set_conn_flag_munmap(gand_conn_t conn);

#endif	/* INCLUDED_con6ity_h_ */

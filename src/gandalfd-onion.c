/*** gandalfd.c -- rolf and milf accessor
 *
 * Copyright (C) 2011-2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <onion/onion.h>
#include <onion/dict.h>
#include <tcbdb.h>
#include "nifty.h"

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

typedef TCBDB *dict_t;
typedef unsigned int dict_id_t;

static dict_t gsymdb;


static dict_t
make_dict(const char *fn, int oflags)
{
	int omode = BDBOREADER;
	dict_t res;

	if (oflags & O_RDWR) {
		omode |= BDBOWRITER;
	}
	if (oflags & O_CREAT) {
		omode |= BDBOCREAT;
	}

	if (UNLIKELY((res = tcbdbnew()) == NULL)) {
		goto out;
	} else if (UNLIKELY(!tcbdbopen(res, fn, omode))) {
		goto free_out;
	}

	/* success, just return the handle we've got */
	return res;

free_out:
	tcbdbdel(res);
out:
	return NULL;
}

static void
free_dict(dict_t d)
{
	tcbdbclose(d);
	tcbdbdel(d);
	return;
}

static dict_id_t
get_sym(dict_t d, const char sym[static 1U], size_t ssz)
{
	const dict_id_t *rp;
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(d, sym, ssz, rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	}
	return *rp;
}


static int
get_nfo(void *UNUSED(p), onion_request *req, onion_response *res)
{
	const char _sym[] = "sym";
	const char *sym;
	dict_id_t sid;

	if ((sym = onion_request_get_query(req, _sym)) == NULL) {
		onion_response_printf(res, "no symbol given");
	} else if (!(sid = get_sym(gsymdb, sym, strlen(sym)))) {
		onion_response_printf(res, "symbol not found");
	} else {
		onion_response_printf(res, "%08u\n", sid);
	}
	return OCS_PROCESSED;
}

static int
get_ser(void *UNUSED(p), onion_request *req, onion_response *res)
{
	const char _sym[] = "sym";
	const char *sym;
	dict_id_t sid;

	if ((sym = onion_request_get_query(req, _sym)) == NULL) {
		onion_response_printf(res, "no symbol given");
	} else if (!(sid = get_sym(gsymdb, sym, strlen(sym)))) {
		onion_response_printf(res, "symbol not found");
	} else {
		onion_response_printf(res, "%08u\n", sid);
	}
	return OCS_PROCESSED;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "gandalfd.xh"
#include "gandalfd.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char **argv[])
{
	onion *o = NULL;
	onion_url *u;
	int res = 0;

	auto void shutdown_server(int UNUSED(_))
	{
		if (o) {
			onion_listen_stop(o);
		}
		return;
	}

	if ((gsymdb = make_dict("gand_idx2sym.tcb", O_RDONLY)) == NULL) {
		fputs("cannot open symbol index file\n", stderr);
		res = 1;
		goto out;
	}

	signal(SIGINT, shutdown_server);
	signal(SIGTERM, shutdown_server);

	o = onion_new(O_POOL);
	onion_set_timeout(o, 1000);
	onion_set_hostname(o, "::");
	
	u = onion_root_url(o);
	onion_url_add(u, "^info", get_nfo);
	onion_url_add(u, "^series", get_ser);

	onion_listen(o);
	onion_free(o);

	free_dict(gsymdb);
out:
	return res;
}

/* gandalfd-onion.c ends here */

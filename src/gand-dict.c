/*** gand-dict.c -- dict reading from key/value store or triplestore
 *
 * Copyright (C) 2009-2014 Sebastian Freundt
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#define USE_REDLAND
#if defined USE_REDLAND
# include <redland.h>
#else
# include <tcbdb.h>
#endif
#include "gand-dict.h"
#include "nifty.h"

#if defined USE_REDLAND
static librdf_world *wrld;
static librdf_storage *stor;
#endif	/* USE_REDLAND */


dict_t
open_dict(const char *fn, int oflags)
{
#if defined USE_REDLAND
	(void)oflags;

	if (wrld == NULL) {
		wrld = librdf_new_world();
	}
	stor = librdf_new_storage(
		wrld, "hashes", fn, "hash-type='bdb',dir='.'");
	return librdf_new_model(wrld, stor, NULL);
#else
	int omode = BDBOREADER;
	dict_t res;

	if (oflags & O_RDWR) {
		omode |= BDBOWRITER;
	}
	if (oflags & O_TRUNC) {
		omode |= BDBOTRUNC;
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
#endif
}

void
close_dict(dict_t d)
{
#if defined USE_REDLAND
	librdf_free_model(d);
	librdf_free_storage(stor);
	librdf_free_world(wrld);
	wrld = NULL;
#else
	tcbdbclose(d);
	tcbdbdel(d);
#endif	/* USE_REDLAND */
	return;
}

dict_oid_t
dict_sym2oid(dict_t d, const char sym[static 1U], size_t ssz)
{
#if defined USE_REDLAND
	static const unsigned char _s[] = "http://lakshmi:8080/v0/series/";
	static const unsigned char _v[] = "http://www.ga-group.nl/rolf/1.0/rid";
	static unsigned char *rs;
	static size_t rz;
	librdf_node *s, *p;
	dict_oid_t rid = 0;

	if (UNLIKELY(rs == NULL)) {
		rz = sizeof(_s) + ssz + 64U;
		rz &= ~(64U - 1U);
		rs = malloc(rz);
		/* prefix with _s */
		memcpy(rs, _s, sizeof(_s));
	} else if (UNLIKELY(sizeof(_s) + ssz > rz)) {
		rz = sizeof(_s) + ssz + 64U;
		rz &= ~(64U - 1U);
		rs = realloc(rs, rz);
	}
	memcpy(rs + sizeof(_s) - 1U, sym, ssz);
	rs[sizeof(_s) + ssz - 1U] = '\0';

	s = librdf_new_node_from_uri_string(wrld, rs);
	p = librdf_new_node_from_uri_string(wrld, _v);

	with (librdf_node *res = librdf_model_get_target(d, s, p)) {
		if (LIKELY(res != NULL)) {
			const unsigned char *val =
				librdf_node_get_literal_value(res);
			rid = strtoul((const char*)val, NULL, 10);
		}
	}
	return rid;

#else
	const dict_oid_t *rp;
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(d, sym, ssz, rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	}
	return *rp;
#endif
}

/* gand-dict.c ends here */

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
#include <stdbool.h>
#include <fcntl.h>
#if defined USE_REDLAND
# include <redland.h>
#else
# include <tcbdb.h>
#endif
#include "gand-dict.h"
#include "nifty.h"

#if !defined USE_REDLAND
# define SID_SPACE	"\x1d"
# define SYM_SPACE	"\x20"
#endif	/* USE_REDLAND */

#if defined USE_REDLAND
static inline librdf_world*
dict_world(bool freep)
{
/* singleton */
	static librdf_world *wrld;

	if (UNLIKELY(wrld == NULL)) {
		wrld = librdf_new_world();
	} else if (UNLIKELY(freep)) {
		librdf_free_world(wrld);
		wrld = NULL;
	}
	return wrld;
}

static inline librdf_uri*
xsdint_uri(bool freep)
{
/* singleton */
	static librdf_uri *xsdint;
	librdf_world *w;

	if (UNLIKELY(xsdint == NULL) &&
	    LIKELY((w = dict_world(false)) != NULL)) {
		xsdint = librdf_new_uri(
			w, "http://www.w3.org/2001/XMLSchema/integer");
	} else if (LIKELY(xsdint != NULL) && UNLIKELY(freep)) {
		librdf_free_uri(xsdint);
		xsdint = NULL;
	}
	return xsdint;
}

static inline librdf_node*
dict_rid(bool freep)
{
/* singleton */
	static const unsigned char _v[] = "http://www.ga-group.nl/rolf/1.0/rid";
	static librdf_node *n;
	librdf_world *w;

	if (UNLIKELY(n == NULL) && LIKELY((w = dict_world(false)) != NULL)) {
		n = librdf_new_node_from_uri_string(w, _v);
	} else if (LIKELY(n != NULL) && UNLIKELY(freep)) {
		librdf_free_node(n);
		n = NULL;
	}
	return n;
}

static inline librdf_node*
dict_max(bool freep)
{
/* singleton */
	static const unsigned char _v[] =
		"http://http://www.w3.org/2001/XMLSchema/maxExclusive";
	static librdf_node *n;
	librdf_world *w;

	if (UNLIKELY(n == NULL) && LIKELY((w = dict_world(false)) != NULL)) {
		n = librdf_new_node_from_uri_string(w, _v);
	} else if (LIKELY(n != NULL) && UNLIKELY(freep)) {
		librdf_free_node(n);
		n = NULL;
	}
	return n;
}

static const unsigned char*
get_symuri(const char *sym, size_t ssz)
{
	static const unsigned char _s[] = "http://lakshmi:8080/v0/series/";
	static unsigned char *rs;
	static size_t rz;

	if (UNLIKELY(sym == NULL)) {
		/* clean up */
		if (rs != NULL) {
			free(rs);
		}
		rs = NULL;
		rz = 0UL;
		return NULL;
	} else if (UNLIKELY(rs == NULL)) {
		rz = sizeof(_s) + ssz + 64U;
		rz &= ~(64UL - 1UL);
		rs = malloc(rz);
		/* prefix with _s */
		memcpy(rs, _s, sizeof(_s));
	} else if (UNLIKELY(sizeof(_s) + ssz > rz)) {
		rz = sizeof(_s) + ssz + 64U;
		rz &= ~(64UL - 1UL);
		rs = realloc(rs, rz);
	}
	/* just to check if the malloc'ing worked */
	if (UNLIKELY(rs == NULL)) {
		rz = 0UL;
		return NULL;
	}
	memcpy(rs + sizeof(_s) - 1U, sym, ssz);
	rs[sizeof(_s) + ssz - 1U] = '\0';
	return rs;
}

static inline librdf_node*
dict_sym(const char sym[static 1U], size_t ssz)
{
/* convenience */
	librdf_world *w;
	const unsigned char *rs;

	if (UNLIKELY((w = dict_world(false)) == NULL)) {
		return NULL;
	} else if (UNLIKELY((rs = get_symuri(sym, ssz)) == NULL)) {
		return NULL;
	}
	return librdf_new_node_from_uri_string(w, rs);
}

#endif	/* USE_REDLAND */


dict_t
open_dict(const char *fn, int oflags)
{
#if defined USE_REDLAND
	char sfl[64U] = "hash-type='bdb'";
	char *sp = sfl + 15U;
	dict_t res = NULL;
	librdf_world *w;

	if (!(oflags & O_RDWR)) {
		memcpy(sp, ",write='no'", 11U + 1U/*\nul*/);
		sp += 11U;
	}
	if (oflags & O_TRUNC) {
		memcpy(sp, ",new='yes'", 10U + 1U/*\nul*/);
		sp += 10U;
	}

	if (UNLIKELY((w = dict_world(false)) == NULL)) {
		return NULL;
	}
	with (librdf_storage *s = librdf_new_storage(w, "hashes", fn, sfl)) {
		res = librdf_new_model(w, s, NULL);
		librdf_free_storage(s);
	}
	return res;
#else  /* !USE_REDLAND */
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
#endif	/* USE_REDLAND */
}

void
close_dict(dict_t d)
{
#if defined USE_REDLAND
	librdf_free_model(d);
	(void)dict_world(true);
#else  /* !USE_REDLAND */
	tcbdbclose(d);
	tcbdbdel(d);
#endif	/* USE_REDLAND */
	return;
}

dict_oid_t
dict_get_sym(dict_t d, const char sym[static 1U], size_t ssz)
{
#if defined USE_REDLAND
	librdf_world *w;
	librdf_node *s, *p;
	dict_oid_t rid = NUL_OID;

	if (UNLIKELY((w = dict_world(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((p = dict_rid(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((s = dict_sym(sym, ssz)) == NULL)) {
		return NUL_OID;
	}

	with (librdf_node *res = librdf_model_get_target(d, s, p)) {
		if (LIKELY(res != NULL)) {
			const unsigned char *val =
				librdf_node_get_literal_value(res);
			rid = strtoul((const char*)val, NULL, 10);
		}
	}
	librdf_free_node(s);
	return rid;

#else  /* !USE_REDLAND */
	const dict_oid_t *rp;
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(d, sym, ssz, rz)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return NUL_OID;
	}
	return *rp;
#endif	/* USE_REDLAND */
}

dict_oid_t
dict_put_sym(dict_t d, const char sym[static 1U], size_t ssz, dict_oid_t id)
{
#if defined USE_REDLAND
	librdf_world *w;
	librdf_uri *i;
	librdf_node *nv;
	const unsigned char *rs;
	unsigned char o[16U];

	if (UNLIKELY((w = dict_world(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((i = xsdint_uri(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((nv = dict_rid(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((rs = get_symuri(sym, ssz)) == NULL)) {
		return NUL_OID;
	}

	snprintf((char*)o, sizeof(o), "%08u", id);

	with (librdf_node *ns = librdf_new_node_from_uri_string(w, rs),
	      *no = librdf_new_node_from_typed_literal(w, o, NULL, i)) {
		librdf_statement *st;

		st = librdf_new_statement_from_nodes(w, ns, nv, no);
		librdf_model_add_statement(d, st);
		librdf_free_statement(st);
		librdf_free_node(ns);
		librdf_free_node(no);
	}
	return id;

#else  /* !USE_REDLAND */
	if (tcbdbput(d, sym, ssz, &id, sizeof(sid)) <= 0) {
		return 0;
	}
	return id;
#endif	/* USE_REDLAND */
}

dict_oid_t
dict_next_oid(dict_t d)
{
#if defined USE_REDLAND
	librdf_world *w;
	librdf_node *s, *p;
	dict_oid_t rid = NUL_OID;

	if (UNLIKELY((w = dict_world(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((s = dict_rid(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((p = dict_max(false)) == NULL)) {
		return NUL_OID;
	}

	with (librdf_node *res = librdf_model_get_target(d, s, p)) {
		if (LIKELY(res != NULL)) {
			const unsigned char *val =
				librdf_node_get_literal_value(res);
			rid = strtoul((const char*)val, NULL, 10);
		}
	}
	return rid;

#else  /* !USE_REDLAND */
	static const char sid[] = SID_SPACE;
	int res;

	if (UNLIKELY((res = tcbdbaddint(d, sid, sizeof(sid), 1)) <= 0)) {
		return 0U;
	}
	return (dict_id_t)res;
#endif	/* USE_REDLAND */
}

dict_oid_t
dict_set_next_oid(dict_t d, dict_oid_t oid)
{
#if defined USE_REDLAND
	librdf_world *w;
	librdf_uri *i;
	librdf_node *s, *p;
	unsigned char o[16U];

	if (UNLIKELY((w = dict_world(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((i = xsdint_uri(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((s = dict_rid(false)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY((p = dict_max(false)) == NULL)) {
		return NUL_OID;
	}
	/* print off oid */
	snprintf((char*)o, sizeof(o), "%08u", oid);

	with (librdf_node *no =
	      librdf_new_node_from_typed_literal(w, o, NULL, i)) {
		librdf_statement *st;

		st = librdf_new_statement_from_nodes(w, s, p, no);
		librdf_model_add_statement(d, st);
		librdf_free_statement(st);
		librdf_free_node(no);
	}

#else  /* !USE_REDLAND */
	static const char sid[] = SID_SPACE;

	tcbdbput(d, sid, sizeof(sid), &oid, sizeof(oid));
#endif	/* USE_REDLAND */

	return oid;
}

/* gand-dict.c ends here */

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

#if defined USE_REDLAND
static librdf_world *wrld;
static librdf_uri *uri_xsdint;
static librdf_uri *uri_rid;
static librdf_uri *uri_max;
static librdf_uri *uri_ser;
#else  /* !USE_REDLAND */
# define SID_SPACE	"\x1d"
# define SYM_SPACE	"\x20"
#endif	/* USE_REDLAND */

#if defined USE_REDLAND
static int
init_world(void)
{
	if (LIKELY(wrld != NULL)) {
		return 0;
	}
	wrld = librdf_new_world();
	uri_xsdint = librdf_new_uri(
		wrld, "http://www.w3.org/2001/XMLSchema/integer");
	uri_rid = librdf_new_uri(
		wrld, "http://www.ga-group.nl/rolf/1.0/rid");
	uri_max = librdf_new_uri(
		wrld, "http://http://www.w3.org/2001/XMLSchema/maxInclusive");
	uri_ser = librdf_new_uri(
		wrld, "http://rolf.ga-group.nl/v0/series/");
	return 0;
}

static int
fini_world(void)
{
	if (UNLIKELY(wrld == NULL)) {
		return 0;
	}
	librdf_free_uri(uri_xsdint);
	librdf_free_uri(uri_rid);
	librdf_free_uri(uri_max);
	librdf_free_uri(uri_ser);
	librdf_free_world(wrld);
	wrld = NULL;
	uri_xsdint = NULL;
	uri_rid = NULL;
	uri_max = NULL;
	uri_ser = NULL;
	return 0;
}

static inline librdf_node*
dict_sym(const char *sym)
{
/* convenience */
	const unsigned char *sp = (const unsigned char*)sym;
	return librdf_new_node_from_uri_local_name(wrld, uri_ser, sp);
}

#endif	/* USE_REDLAND */


dict_t
open_dict(const char *fn, int oflags)
{
#if defined USE_REDLAND
	char sfl[64U] = "hash-type='bdb'";
	char *sp = sfl + 15U;
	dict_t res = NULL;

	if (!(oflags & O_RDWR)) {
		memcpy(sp, ",write='no'", 11U + 1U/*\nul*/);
		sp += 11U;
	}
	if (oflags & O_TRUNC) {
		memcpy(sp, ",new='yes'", 10U + 1U/*\nul*/);
		sp += 10U;
	}

	if (UNLIKELY(init_world() < 0)) {
		return NULL;
	}
	with (librdf_storage *s = librdf_new_storage(wrld, "hashes", fn, sfl)) {
		res = librdf_new_model(wrld, s, NULL);
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
	(void)fini_world();
#else  /* !USE_REDLAND */
	tcbdbclose(d);
	tcbdbdel(d);
#endif	/* USE_REDLAND */
	return;
}

dict_oid_t
dict_get_sym(dict_t d, const char *sym)
{
#if defined USE_REDLAND
	librdf_node *s, *p;
	dict_oid_t rid = NUL_OID;

	if (UNLIKELY((s = dict_sym(sym)) == NULL)) {
		return NUL_OID;
	}

	p = librdf_new_node_from_uri(wrld, uri_rid);
	with (librdf_node *res = librdf_model_get_target(d, s, p)) {
		if (LIKELY(res != NULL)) {
			const unsigned char *val =
				librdf_node_get_literal_value(res);
			rid = strtoul((const char*)val, NULL, 10);
		}
	}
	librdf_free_node(s);
	librdf_free_node(p);
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
dict_put_sym(dict_t d, const char *sym, dict_oid_t id)
{
#if defined USE_REDLAND
	librdf_node *ns, *nv, *no;
	unsigned char o[16U];

	if (UNLIKELY((ns = dict_sym(sym)) == NULL)) {
		return NUL_OID;
	}

	snprintf((char*)o, sizeof(o), "%08u", id);
	nv = librdf_new_node_from_uri(wrld, uri_rid);
	no = librdf_new_node_from_typed_literal(wrld, o, NULL, uri_xsdint);
	with (librdf_statement *st) {
		st = librdf_new_statement_from_nodes(wrld, ns, nv, no);
		librdf_model_add_statement(d, st);
		librdf_free_statement(st);
	}
	librdf_free_node(ns);
	librdf_free_node(nv);
	librdf_free_node(no);
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
	librdf_node *s, *p;
	dict_oid_t rid = NUL_OID;

	s = librdf_new_node_from_uri(wrld, uri_rid);
	p = librdf_new_node_from_uri(wrld, uri_max);

	with (librdf_node *res = librdf_model_get_target(d, s, p)) {
		if (LIKELY(res != NULL)) {
			const unsigned char *val =
				librdf_node_get_literal_value(res);
			if ((rid = strtoul((const char*)val, NULL, 10))) {
				rid++;
			}
		}
	}
	librdf_free_node(s);
	librdf_free_node(p);
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
	librdf_node *s, *p, *o;
	unsigned char _o[16U];

	s = librdf_new_node_from_uri(wrld, uri_rid);
	p = librdf_new_node_from_uri(wrld, uri_max);
	/* print off oid */
	snprintf((char*)_o, sizeof(_o), "%08u", oid);
	o = librdf_new_node_from_typed_literal(wrld, _o, NULL, uri_xsdint);

	with (librdf_statement *st) {
		st = librdf_new_statement_from_nodes(wrld, s, p, o);
		librdf_model_add_statement(d, st);
		librdf_free_statement(st);
	}
	librdf_free_node(s);
	librdf_free_node(p);
	librdf_free_node(o);

#else  /* !USE_REDLAND */
	static const char sid[] = SID_SPACE;

	tcbdbput(d, sid, sizeof(sid), &oid, sizeof(oid));
#endif	/* USE_REDLAND */

	return oid;
}

/* gand-dict.c ends here */

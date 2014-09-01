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
static librdf_uri *vrb_rid;
static librdf_uri *vrb_src;
static librdf_uri *uri_max;
static librdf_uri *uri_ser;
static librdf_uri *uri_src;
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
	vrb_rid = librdf_new_uri(
		wrld, "http://www.ga-group.nl/rolf/1.0/rid");
	vrb_src = librdf_new_uri(
		wrld, "http://www.ga-group.nl/rolf/1.0/src");
	uri_max = librdf_new_uri(
		wrld, "http://http://www.w3.org/2001/XMLSchema/maxInclusive");
	uri_ser = librdf_new_uri(
		wrld, "http://rolf.ga-group.nl/v0/series/");
	uri_src = librdf_new_uri(
		wrld, "http://rolf.ga-group.nl/v0/sources/");
	return 0;
}

static int
fini_world(void)
{
	if (UNLIKELY(wrld == NULL)) {
		return 0;
	}
	librdf_free_uri(uri_xsdint);
	librdf_free_uri(vrb_rid);
	librdf_free_uri(vrb_src);
	librdf_free_uri(uri_max);
	librdf_free_uri(uri_ser);
	librdf_free_uri(uri_src);
	librdf_free_world(wrld);
	wrld = NULL;
	uri_xsdint = NULL;
	vrb_rid = NULL;
	vrb_src = NULL;
	uri_max = NULL;
	uri_ser = NULL;
	uri_src = NULL;
	return 0;
}

static inline librdf_node*
dict_sym(const char *sym)
{
/* convenience */
	const unsigned char *sp = (const unsigned char*)sym;
	return librdf_new_node_from_uri_local_name(wrld, uri_ser, sp);
}

static inline librdf_node*
dict_src(const char *src)
{
/* convenience */
	const unsigned char *sp = (const unsigned char*)src;
	return librdf_new_node_from_uri_local_name(wrld, uri_src, sp);
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

	p = librdf_new_node_from_uri(wrld, vrb_rid);
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
	const size_t ssz = strlen(sym);
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
dict_put_sym(dict_t d, const char *sym, dict_oid_t sid)
{
#if defined USE_REDLAND
	librdf_node *s, *p, *o;
	unsigned char _o[16U];

	if (UNLIKELY((s = dict_sym(sym)) == NULL)) {
		return NUL_OID;
	}

	p = librdf_new_node_from_uri(wrld, vrb_rid);

	/* prep the sid literal */
	snprintf((char*)_o, sizeof(_o), "%08u", sid);
	o = librdf_new_node_from_typed_literal(wrld, _o, NULL, uri_xsdint);

	with (librdf_statement *st) {
		st = librdf_new_statement_from_nodes(wrld, s, p, o);
		librdf_model_add_statement(d, st);
		librdf_free_statement(st);
	}

	with (const char *x = strrchr(sym, '@')) {
		if (UNLIKELY(x == NULL)) {
			break;
		} else if (UNLIKELY((s = dict_sym(sym)) == NULL)) {
			break;
		} else if (UNLIKELY((o = dict_src(x + 1U)) == NULL)) {
			break;
		}

		p = librdf_new_node_from_uri(wrld, vrb_src);
		with (librdf_statement *st) {
			st = librdf_new_statement_from_nodes(wrld, s, p, o);
			librdf_model_add_statement(d, st);
			librdf_free_statement(st);
		}
	}
	return sid;

#else  /* !USE_REDLAND */
	const size_t ssz = strlen(sym);

	if (tcbdbput(d, sym, ssz, &sid, sizeof(sid)) <= 0) {
		return 0;
	}
	return sid;
#endif	/* USE_REDLAND */
}

dict_oid_t
dict_next_oid(dict_t d)
{
#if defined USE_REDLAND
	librdf_node *s, *p;
	dict_oid_t rid = NUL_OID;

	s = librdf_new_node_from_uri(wrld, vrb_rid);
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
	return (dict_oid_t)res;
#endif	/* USE_REDLAND */
}

dict_oid_t
dict_set_next_oid(dict_t d, dict_oid_t oid)
{
#if defined USE_REDLAND
	librdf_node *s, *p, *o;
	unsigned char _o[16U];

	s = librdf_new_node_from_uri(wrld, vrb_rid);
	p = librdf_new_node_from_uri(wrld, uri_max);
	/* print off oid */
	snprintf((char*)_o, sizeof(_o), "%08u", oid);
	o = librdf_new_node_from_typed_literal(wrld, _o, NULL, uri_xsdint);

	with (librdf_statement *st) {
		st = librdf_new_statement_from_nodes(wrld, s, p, o);
		librdf_model_add_statement(d, st);
		librdf_free_statement(st);
	}

#else  /* !USE_REDLAND */
	static const char sid[] = SID_SPACE;

	tcbdbput(d, sid, sizeof(sid), &oid, sizeof(oid));
#endif	/* USE_REDLAND */

	return oid;
}


/* iterators */
dict_si_t
dict_sym_iter(dict_t d)
{
/* uses static state */
#if defined USE_REDLAND
	static librdf_stream *i;
	static const char *pre;
	static size_t prz;
	librdf_statement *st;
	librdf_node *s, *o;
	dict_si_t res;

	if (UNLIKELY(i == NULL)) {
		librdf_node *p = librdf_new_node_from_uri(wrld, vrb_rid);

		st = librdf_new_statement_from_nodes(wrld, NULL, p, NULL);
		i = librdf_model_find_statements(d, st);
		librdf_free_statement(st);

		/* while we're at it */
		pre = (const char*)librdf_uri_as_counted_string(uri_ser, &prz);
	} else {
		(void)librdf_stream_next(i);
	}

	if (UNLIKELY((st = librdf_stream_get_object(i)) == NULL)) {
		goto null;
	}
	s = librdf_statement_get_subject(st);
	o = librdf_statement_get_object(st);

	with (librdf_uri *u = librdf_node_get_uri(s)) {
		const char *ustr = (const char*)librdf_uri_as_string(u);

		if (LIKELY(!strncmp(ustr, pre, prz))) {
			ustr += prz;
		}
		res.sym = ustr;
	}
	with (const unsigned char *val = librdf_node_get_literal_value(o)) {
		res.sid = strtoul((const char*)val, NULL, 10);
	}
	return res;

null:
	if (LIKELY(i != NULL)) {
		librdf_free_stream(i);
	}
	i = NULL;
	return (dict_si_t){};

#else  /* !USE_REDLAND */
	static BDBCUR *c;
	dict_si_t res;
	const void *vp;
	int z[1U];

	if (UNLIKELY(c == NULL)) {
		c = tcbdbcurnew(d);
		tcbdbcurjump(c, SYM_SPACE, sizeof(SYM_SPACE));
	}

	if (UNLIKELY((vp = tcbdbcurval3(c, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(int)) {
		goto null;
	}
	/* otherwise fill res */
	res.sid = *(const int*)vp;

	if (UNLIKELY((vp = tcbdbcurkey3(c, z)) == NULL)) {
		goto null;
	}
	/* or fill */
	res.sym = vp;
	/* also iterate to the next thing */
	tcbdbcurnext(c);
	return res;

null:
	if (LIKELY(c != NULL)) {
		tcbdbcurdel(c);
	}
	c = NULL;
	return (dict_si_t){};
#endif	/* USE_REDLAND */
}

dict_si_t
dict_src_iter(dict_t d, const char *src)
{
/* uses static state */
#if defined USE_REDLAND
	static librdf_iterator *i;
	static const char *pre;
	static size_t prz;
	librdf_node *s;
	dict_si_t res;

	if (UNLIKELY(i == NULL)) {
		librdf_node *p = librdf_new_node_from_uri(wrld, vrb_src);
		librdf_node *o = dict_src(src);

		i = librdf_model_get_sources(d, p, o);

		/* while we're at it */
		pre = (const char*)librdf_uri_as_counted_string(uri_ser, &prz);
	} else {
		(void)librdf_iterator_next(i);
	}

	if (UNLIKELY((s = librdf_iterator_get_object(i)) == NULL)) {
		goto null;
	}

	with (librdf_uri *u = librdf_node_get_uri(s)) {
		const char *ustr = (const char*)librdf_uri_as_string(u);

		if (LIKELY(!strncmp(ustr, pre, prz))) {
			ustr += prz;
		}
		res.sym = ustr;
	}
	res.sid = 1U;
	return res;

null:
	if (LIKELY(i != NULL)) {
		librdf_free_iterator(i);
	}
	i = NULL;
	return (dict_si_t){};
#else  /* !USE_REDLAND */
	(void)d;
	(void)src;

	return (dict_si_t){};
#endif	/* USE_REDLAND */
}

/* gand-dict.c ends here */

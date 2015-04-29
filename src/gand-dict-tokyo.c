/*** gand-dict-tokyo.c -- dict reading from key/value store
 *
 * Copyright (C) 2009-2015 Sebastian Freundt
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
#if !defined USE_TOKYOCABINET
# error tokyocabinet database backend not available
#endif	/* !USE_TOKYOCABINET */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <tcbdb.h>
#include "gand-dict.h"
#include "nifty.h"

#define SID_SPACE	"\x1d"
#define SYM_SPACE	"\x20"


dict_t
open_dict(const char *fn, int oflags)
{
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
}

void
close_dict(dict_t d)
{
	tcbdbclose(d);
	tcbdbdel(d);
	return;
}

dict_oid_t
dict_get_sym(dict_t d, const char *sym)
{
	const dict_oid_t *rp;
	const size_t ssz = strlen(sym);
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(d, sym, ssz, rz)) == NULL)) {
		return NUL_OID;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return NUL_OID;
	}
	return *rp;
}

dict_oid_t
dict_put_sym(dict_t d, const char *sym, dict_oid_t sid)
{
	const size_t ssz = strlen(sym);

	if (tcbdbput(d, sym, ssz, &sid, sizeof(sid)) <= 0) {
		return 0;
	}
	return sid;
}

dict_oid_t
dict_next_oid(dict_t d)
{
	static const char sid[] = SID_SPACE;
	int res;

	if (UNLIKELY((res = tcbdbaddint(d, sid, sizeof(sid), 1)) <= 0)) {
		return 0U;
	}
	return (dict_oid_t)res;
}

dict_oid_t
dict_set_next_oid(dict_t d, dict_oid_t oid)
{
	static const char sid[] = SID_SPACE;

	tcbdbput(d, sid, sizeof(sid), &oid, sizeof(oid));
	return oid;
}


/* iterators */
dict_si_t
dict_sym_iter(dict_t d)
{
/* uses static state */
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
}

dict_si_t
dict_src_iter(dict_t d, const char *src)
{
/* uses static state */
	(void)d;
	(void)src;

	return (dict_si_t){};
}

/* gand-dict-tokyo.c ends here */

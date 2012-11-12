/*** slut.c -- symbol look-up table
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of uterus/gandalf.
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
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include "nifty.h"
/* symbol table stuff */
#include "slut-trie-glue.h"
#include "slut.h"
#include "fileutils.h"

#define IGNCASE_CHAR	('\t')

static size_t
next_2pow(size_t idx)
{
	if (idx == 0UL) {
		return 0UL;
	} else if (idx < 4096UL) {
		return 4096UL;
	} else if (idx < 65536UL) {
		return 65536UL;
	} else if (idx < 262144UL) {
		return 262144UL;
	} else if (idx < 1048576UL) {
		return 1048576UL;
	} else if (idx < 4194304UL) {
		return 4194304UL;
	} else {
		size_t x2p = 4194304UL * 2;

		for (x2p = 4194304UL * 2; x2p < idx; x2p *= 2);
		return x2p;
	}
}

static void
check_rtblz(slut_t s, size_t at_least)
{
	size_t o_least = slut_nsyms(s);

	o_least = next_2pow(o_least);
	at_least = next_2pow(at_least);

	if (UNLIKELY(s->rtbl == NULL)) {
		size_t newz = at_least * sizeof(*s->rtbl);
		s->rtbl = mmap(NULL, newz, PROT_MEM, MAP_MEM, -1, 0);
	} else if (o_least < at_least) {
		size_t oldz = o_least * sizeof(*s->rtbl);
		size_t newz = at_least * sizeof(*s->rtbl);
		s->rtbl = mremap(s->rtbl, oldz, newz, MREMAP_MAYMOVE);
	}
	return;
}


DEFUN void
make_slut(slut_t s)
{
	/* nope, nothing yet */
	s->nsyms = 0UL;
	/* init the s2i trie */
	s->stbl = make_slut_tg();
	/* init the rtbl (resolves rid -> data) */
	s->rtbl = NULL;
	/* make sure we can take 4096 rids */
	check_rtblz(s, 1U);
	return;
}

DEFUN void
free_slut(slut_t s)
{
	/* s2i */
	if (s->stbl != NULL) {
		free_slut_tg(s->stbl);
		s->stbl = NULL;
	}
	return;
}

DEFUN rid_t
slut_sym2rid(slut_t s, const char *sym)
{
	trie_data_t data[1];

	/* make an alpha char array first */
	if (slut_tg_get(s->stbl, sym, data) < 0) {
		/* create a new entry */
		return 0UL;
	}
	return (rid_t)*data;
}

DEFUN rid_t
slut_isym2rid(slut_t s, const char *sym)
{
	static char isym[64];
	trie_data_t data[1];

	/* indicator for ignore case branch */
	isym[0] = IGNCASE_CHAR;
	{
		char *p = isym + 1;
		for (const char *q = sym, *ep = isym + sizeof(isym);
		     p < ep && (*p++ = tolower(*q++)););
		*p = '\0';
	}

	/* ordinary lookup now */
	if (slut_tg_get(s->stbl, isym, data) < 0) {
		/* create a new entry */
		return 0UL;
	}
	return (rid_t)*data;
}

DEFUN struct slut_data_s
slut_rid2data(slut_t s, rid_t rid)
{
	if (LIKELY(rid <= slut_nsyms(s))) {
		return s->rtbl[rid - 1];
	}
	return slut_data_initialiser();
}

DEFUN int
slut_put(slut_t s, const char *sym, rid_t rid, struct slut_data_s data)
{
	if (UNLIKELY(rid == 0)) {
		return -1;
	}
	slut_tg_put(s->stbl, sym, rid);

	check_rtblz(s, rid);
	s->rtbl[rid - 1] = data;

	if (UNLIKELY(rid > s->nsyms)) {
		s->nsyms = rid;
	}
	return 0;
}

DEFUN int
slut_load(slut_t s, const char *fn)
{
	struct mmfb_s fb;

	if (mmap_whole_file(&fb, fn) < 0) {
		return -1;
	}

	/* go through all lines of fb.m.buf */
	for (const char *ln = fb.m.buf,
		     *ep = ln + fb.m.bsz,
		     *eoln;
	     ln < ep; ln = (eoln ?: ep) + 1) {
		struct slut_data_s sd;
		rid_t rid;
		char *p;

		/* look for the end of the line */
		eoln = memchr(ln, '\n', ep - ln);

		/* fill in the slut data object */
		sd.beg = ln - fb.m.buf;
		sd.end = eoln - fb.m.buf;
		if ((rid = strtoul(ln, &p, 10)) &&
		    p != NULL && *p == '\t') {
			/* yaay, send him off, though snarf the symbol first */
			char sym[64], *q = sym + 1;

			while (*++p != '\t');
			while ((*q++ = *++p) != '\t');
			*--q = '\0';

			slut_put(s, sym + 1, rid, sd);

			/* put the lower case version too */
			sym[0] = IGNCASE_CHAR;
			for (p = sym + 1; p < q; p++) {
				*p = tolower(*p);
			}
			slut_tg_put(s->stbl, sym, rid);
		}
	}

	munmap_all(&fb);
	return 0;
}

/* slut.c ends here */

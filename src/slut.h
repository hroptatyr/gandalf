/*** slut.h -- symbol look-up table
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

#if !defined INCLUDED_slut_h_
# define INCLUDED_slut_h_

#include <stddef.h>
#include <stdint.h>
#include "slut-trie-glue.h"

#undef DECLF
#undef DEFUN
#if defined STATIC_GUTS
# define DEFUN	static
# define DECLF	static
#else  /* !STATIC_GUTS */
# define DEFUN
# define DECLF	extern
#endif	/* STATIC_GUTS */

typedef uint32_t rid_t;

/**
 * Our own data associated with each rid. */
typedef struct slut_data_s *slut_data_t;

typedef struct slut_s *slut_t;

struct slut_data_s {
	off_t beg;
	off_t end;
};

struct slut_s {
	/* sym2rid */
	__slut_t stbl;
	/* rid2data */
	struct slut_data_s *rtbl;
	/* administrative stuff */
	uint32_t alloc_sz;
	uint32_t nsyms;
};

static inline __attribute__((const, pure)) struct slut_data_s
slut_data_initialiser(void)
{
	static const struct slut_data_s res = {0};
	return res;
}

/* ctor and dtor */
DECLF void make_slut(slut_t s);
DECLF void free_slut(slut_t s);

/* accessors */
DECLF rid_t slut_sym2rid(slut_t s, const char *sym);

/* like slut_sym2rid() but ignore case of SYM. */
DECLF rid_t slut_isym2rid(slut_t s, const char *sym);

/**
 * For rolf id RID retrieve data associated in the slut S. */
DECLF struct slut_data_s slut_rid2data(slut_t s, rid_t rid);

/**
 * Store data D under key SYM and rid RID in slut S. */
DECLF int slut_put(slut_t s, const char *sym, rid_t rid, struct slut_data_s d);

/**
 * Load a complete file FN into S. */
DECLF int slut_load(slut_t s, const char *fn);

/**
 * Return the number of symbols currently in the slut S. */
static inline size_t
slut_nsyms(slut_t s)
{
	return s->nsyms;
}

#endif	/* INCLUDED_slut_h_ */

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
#include "triedefs.h"

#undef DECLF
#undef DEFUN
#if defined STATIC_GUTS
# define DEFUN	static
# define DECLF	static
#else  /* !STATIC_GUTS */
# define DEFUN
# define DECLF	extern
#endif	/* STATIC_GUTS */

typedef struct slut_s *slut_t;

/* anonymous glue type, could be a trie */
typedef void *__slut_t;
/* anonymous glue type, could be just an array */
typedef void *__ilut_t;

struct slut_s {
	/* sym2rid */
	__slut_t stbl;
	/* administrative stuff */
	uint32_t alloc_sz;
	uint32_t nsyms;
};

/* (de)initialiser */
DECLF void init_slut(void);
DECLF void fini_slut(void);

/* ctor and dtor */
DECLF void make_slut(slut_t s);
DECLF void free_slut(slut_t s);

/* accessors */
DECLF rid_t slut_sym2rid(slut_t s, const char *sym);

/**
 * For symbol SYM retrieve data associated in the slut S. */
DECLF struct trie_data_s slut_get(slut_t s, const char *sym);

/**
 * Store DATA under key SYM in slut S. */
DECLF int slut_put(slut_t s, const char *sym, struct trie_data_s data);

/**
 * Return the number of symbols currently in the slut S. */
static inline size_t
slut_nsyms(slut_t s)
{
	return s->nsyms;
}

#endif	/* INCLUDED_slut_h_ */

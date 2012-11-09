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
#include "nifty.h"
#include "slut.h"
/* symbol table stuff */
#include "slut-trie-glue.h"


DEFUN void
init_slut(void)
{
	return;
}

DEFUN void
fini_slut(void)
{
	return;
}

DEFUN void
make_slut(slut_t s)
{
	/* singleton */
	init_slut();

	/* init the s2i trie */
	s->stbl = make_slut_tg();
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

static struct trie_data_s
__crea(slut_t s, const char *sym)
{
	struct trie_data_s res = {0};

	/* store in the s2i table (trie) */
	slut_tg_put(s->stbl, sym, res);
	return res;
}

DEFUN rid_t
slut_sym2rid(slut_t s, const char *sym)
{
	struct trie_data_s data[1];

	/* make an alpha char array first */
	if (slut_tg_get(s->stbl, sym, data) < 0) {
		/* create a new entry */
		return 0;
	}
	return data->rid;
}

DEFUN struct trie_data_s
slut_sym2data(slut_t s, const char *sym)
{
	struct trie_data_s data[1];

	/* make an alpha char array first */
	if (slut_tg_get(s->stbl, sym, data) < 0) {
		/* create a new entry */
		return trie_data_error();
	}
	return *data;
}

/* slut.c ends here */

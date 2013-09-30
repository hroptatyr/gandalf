/*** nifty.h -- generally handy macroes
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#if !defined INCLUDED_nifty_h_
#define INCLUDED_nifty_h_

#include <stdlib.h>
#include <unistd.h>

#if !defined index_t
# define index_t	long unsigned int
#endif	/* !index_t */

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */
#define ALGN16(_x)	__attribute__((aligned(16))) _x

#define countof(x)		(sizeof(x) / sizeof(*x))
#define countof_m1(x)		(countof(x) - 1)

/* The encoded parameter sizes will be rounded up to match pointer alignment. */
#if !defined ROUND
# define ROUND(s, a)		(a * ((s + a - 1) / a))
#endif	/* !ROUND */
#if !defined aligned_sizeof
# define aligned_sizeof(t)	ROUND(sizeof(t), __alignof(void*))
#endif	/* !aligned_sizeof */

#if !defined xmalloc
# define xmalloc(_x)	malloc(_x)
#endif	/* !xmalloc */
#if !defined xnew
# define xnew(_a)	xmalloc(sizeof(_a))
#endif	/* !xnew */
#if !defined xfree
# define xfree(_x)	free(_x)
#endif	/* !xfree */

#if defined DEBUG_FLAG
# include <stdio.h>
# define GAND_DEBUG(args...)			\
	fprintf(gand_logout, "[gand] " args)
# define GAND_DBGCONT(args...)			\
	fprintf(gand_logout, args)
#else  /* !DEBUG_FLAG */
# define GAND_DEBUG(args...)
# define GAND_DBGCONT(args...)
#endif	/* DEBUG_FLAG */

/* fixme, any chance we can get that one from the server? */
extern void *gand_logout;

/* just a service for mmap based allocators */
#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	MAP_ANONYMOUS
#endif	/* MAP_ANON && !MAP_ANONYMOUS */
#if !defined MAP_MEM
# define MAP_MEM	(MAP_PRIVATE | MAP_ANON)
#endif	/* MAP_MEM */
#if !defined PROT_MEM
# define PROT_MEM	(PROT_READ | PROT_WRITE)
#endif	/* PROT_MEM */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#define _paste(x, y)	x ## y
#define paste(x, y)	_paste(x, y)

#if !defined with
# define with(args...)							\
	for (args, *paste(__ep, __LINE__) = (void*)1;			\
	     paste(__ep, __LINE__); paste(__ep, __LINE__)= 0)
#endif	/* !with */

#if !defined if_with
# define if_with(init, args...)					\
	for (init, *paste(__ep, __LINE__) = (void*)1;			\
	     paste(__ep, __LINE__) && (args); paste(__ep, __LINE__)= 0)
#endif	/* !if_with */

#define once					\
	static int paste(__, __LINE__);		\
	if (!paste(__, __LINE__)++)
#define but_first				\
	static int paste(__, __LINE__);		\
	if (paste(__, __LINE__)++)

#endif	/* INCLUDED_nifty_h_ */

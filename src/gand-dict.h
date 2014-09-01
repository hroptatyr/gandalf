/*** gand-dict.h -- dict reading from key/value store or triplestore
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
#if !defined INCLUDED_gand_dict_h_
#define INCLUDED_gand_dcit_h_

typedef void *dict_t;
typedef unsigned int dict_oid_t;

#define NUL_OID		((dict_oid_t)0U)


extern dict_t open_dict(const char *fn, int oflags);

extern void close_dict(dict_t d);

/**
 * Return oid for SYM (of length SSZ), or NUL_OID if not existent. */
extern dict_oid_t
dict_get_sym(dict_t d, const char sym[static 1U], size_t ssz);

/**
 * Put SYM (of length SSZ) into dictionary D under oid ID, or
 * if ID is NUL_OID, create a suitable oid.
 * Return the oid that SYM is mapped to. */
extern dict_oid_t
dict_put_sym(dict_t d, const char sym[static 1U], size_t ssz, dict_oid_t id);

/**
 * Return the next available oid. */
extern dict_oid_t
dict_next_oid(dict_t d);

/**
 * Clamp next available oid to OID. */
extern dict_oid_t
dict_set_next_oid(dict_t d, dict_oid_t oid);

#endif	/* INCLUDED_gand_dict_h_ */

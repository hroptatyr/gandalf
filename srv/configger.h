/*** configger.h -- config stuff
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
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

#if !defined INCLUDED_configger_h_
#define INCLUDED_configger_h_

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdbool.h>
#include "nifty.h"

/**
 * Generic handle to refer to the configuration somehow. */
typedef void *cfg_t;

/**
 * Opaque data type for settings tables whither configuration goes. */
typedef void *cfgset_t;

/**
 * Read config from file FN and return a configuration context. */
extern cfg_t configger_init(const char *fn);
/**
 * Free all resources associated with a configuration context. */
extern void configger_fini(cfg_t);

/**
 * Return a pointer to the cfgset array and its size. */
extern size_t cfg_get_sets(cfgset_t **tgt, cfg_t);


#if defined USE_LUA
# include "lua-config.h"

/* config mumbojumbo, just redirs to the lua cruft */
static inline cfgset_t
cfg_tbl_lookup(cfg_t ctx, cfgset_t s, const char *name)
{
	return lc_cfgtbl_lookup(ctx, s, name);
}

static inline void
cfg_tbl_free(cfg_t ctx, cfgset_t s)
{
	lc_cfgtbl_free(ctx, s);
	return;
}

static inline size_t
cfg_tbl_lookup_s(const char **t, cfg_t c, cfgset_t s, const char *n)
{
	return lc_cfgtbl_lookup_s(t, c, s, n);
}

static inline int
cfg_tbl_lookup_i(cfg_t c, cfgset_t s, const char *n)
{
	return lc_cfgtbl_lookup_i(c, s, n);
}

static inline size_t
cfg_glob_lookup_s(const char **t, cfg_t c, const char *n)
{
	return lc_globcfg_lookup_s(t, c, n);
}

static inline bool
cfg_glob_lookup_b(cfg_t ctx, const char *name)
{
	return lc_globcfg_lookup_b(ctx, name);
}

static inline int
cfg_glob_lookup_i(cfg_t ctx, const char *name)
{
	return lc_globcfg_lookup_i(ctx, name);
}
#endif	/* USE_LUA */

#endif	/* INCLUDED_configger_h_ */

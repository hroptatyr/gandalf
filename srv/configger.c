/*** configger.c -- config stuff, abstract from lua
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include "configger.h"

#if defined USE_LUA
/* lua bindings */
#include <lua.h>
#include <lauxlib.h>
#include "lua-config.h"

static cfg_t state_singleton = NULL;
static cfgset_t cfgsets[16];
static size_t ncfgsets = 0UL;

size_t
cfg_get_sets(cfgset_t **p, cfg_t UNUSED(L))
{
	*p = cfgsets;
	return ncfgsets;
}

static inline void*
lc_ref(void *L)
{
	int r = luaL_ref(L, LUA_REGISTRYINDEX);
	return (void*)(long int)r;
}

static int
lc_load_module(lua_State *L)
{
	if (!lua_istable(L, 1)) {
		fprintf(stderr, "argument is not a table\n");
		return -1;
	}
	if (ncfgsets < countof(cfgsets)) {
		cfgsets[ncfgsets++] = lc_ref(L);
	}
	return 0;
}

static void
register_funs(cfg_t L)
{
	lua_register(L, "load_module", lc_load_module);
	return;
}

cfg_t
configger_init(const char *file)
{
	if (LIKELY(state_singleton == NULL &&
		   (state_singleton = luaL_newstate()) != NULL)) {
		register_funs(state_singleton);
	}

	if (read_lua_config(state_singleton, file)) {
		return state_singleton;
	}
	return NULL;
}

void
configger_fini(cfg_t c)
{
	lua_close(c);
	state_singleton = NULL;
	return;
}
#endif	/* USE_LUA */

/* configger.c ends here */

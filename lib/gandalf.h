/*** gandalf.h -- fixml pre-trade messages
 *
 * Copyright (C) 2011 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of the army of unserding daemons.
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

#if !defined INCLUDED_gandalf_h_
#define INCLUDED_gandalf_h_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

typedef void *gand_ctx_t;
typedef struct __gand_s *gand_doc_t;
typedef struct gand_msg_s *gand_msg_t;

typedef uint32_t idate_t;

/* message types */
typedef enum {
	GAND_MSG_UNK,
	/* rolf show like messages */
	GAND_MSG_GET_SER,
	GAND_MSG_GET_DAT,
	GAND_MSG_GET_NFO,
} gand_msg_type_t;

struct gand_msg_hdr_s {
	/* this is generally msg_type * 2 */
	unsigned int mt;
};

struct rolf_obj_s {
	uint32_t rolf_id;
	char *rolf_sym;
};

struct date_rng_s {
	idate_t beg;
	idate_t end;
};

struct valflav_s {
	char *this;
	size_t nalts;
	size_t nall_alts;
	char **alts;
};

enum select_e {
	SEL_ALL = -1,
	SEL_NOTHING = 0,
	SEL_SYM = 1,
	SEL_RID = 2,
	SEL_TID = 4,
	SEL_DATE = 8,
	SEL_VFID = 16,
	SEL_VFLAV = 32,
	SEL_VALUE = 64,
};

struct gand_msg_s {
	struct gand_msg_hdr_s hdr;

	size_t nrolf_objs;
	struct rolf_obj_s *rolf_objs;

	size_t ndate_rngs;
	struct date_rng_s *date_rngs;

	size_t nvalflavs;
	struct valflav_s *valflavs;

	uint8_t sel;
};

#define GAND_MSG_ROLF_OBJS_INC	(16)
#define GAND_MSG_DATE_RNGS_INC	(16)
#define GAND_MSG_VALFLAVS_INC	(16)


/* some useful functions */
/**
 * Free resources associated with MSG. */
extern void gand_free_msg(gand_msg_t);

/* blob parsing */
/**
 * Parse BSZ bytes in BUF and, by side effect, obtain a context.
 * That context can be reused in subsequent calls to
 * `gand_parse_blob()' to parse fragments of XML documents in
 * several goes, use a NULL pointer upon the first go.
 * If CTX becomes NULL the document is either finished or
 * errors have occurred, the return value will be the document
 * in the former case or NULL in the latter. */
extern gand_msg_t
gand_parse_blob(gand_ctx_t *ctx, const char *buf, size_t bsz);

/**
 * Like `gand_parse_blob()' but re-entrant (and thus slower). */
extern gand_msg_t
gand_parse_blob_r(gand_ctx_t *ctx, const char *buf, size_t bsz);

/**
 * Set the message type of MSG to MT. */
static inline void
gand_set_msg_type(gand_msg_t msg, gand_msg_type_t mt)
{
	msg->hdr.mt = mt * 2;
	return;
}

/**
 * Get the message type of MSG. */
static inline gand_msg_type_t
gand_get_msg_type(gand_msg_t msg)
{
	return (gand_msg_type_t)(msg->hdr.mt / 2);
}

/* convert an iso date string to an idate */
static inline idate_t
__to_idate(const char *dstr)
{
	char *p;
	uint32_t y = strtoul(dstr, &p, 10);
	uint32_t m = strtoul(p + 1, &p, 10);
	uint32_t d = strtoul(p + 1, &p, 10);
	return y * 10000 + m * 100 + d;
}

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_gandalf_h_ */

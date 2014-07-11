/*** logger.h -- unserding logging service
 *
 * Copyright (C) 2011-2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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

#if !defined INCLUDED_logger_h_
#define INCLUDED_logger_h_

#include <syslog.h>

#define GAND_LOG_FLAGS		(LOG_PID | LOG_NDELAY)
#define GAND_FACILITY		(LOG_LOCAL4)
#define GAND_MAKEPRI(x)		(x)
#define GAND_SYSLOG(x, args...)	gand_log(GAND_MAKEPRI(x), args)


extern void(*gand_log)(int prio, const char *fmt, ...);

extern __attribute__((format(printf, 2, 3))) void
gand_errlog(int prio, const char *fmt, ...);

/* convenience macros */
#define GAND_DEBUG(args...)
#define GAND_DBGCONT(args...)

#if defined GAND_LOG_PREFIX
# define GAND_LOG_XPRE		GAND_LOG_PREFIX " "
#else  /* !GAND_LOG_PREFIX */
# define GAND_LOG_XPRE
#endif	/* GAND_LOG_PREFIX */

#define GAND_INFO_LOG(args...)					\
	do {							\
		GAND_SYSLOG(LOG_INFO, GAND_LOG_XPRE args);	\
		GAND_DEBUG("INFO " args);			\
	} while (0)
#define GAND_ERR_LOG(args...)						\
	do {								\
		GAND_SYSLOG(LOG_ERR, GAND_LOG_XPRE "ERROR " args);	\
		GAND_DEBUG("ERROR " args);				\
	} while (0)
#define GAND_CRIT_LOG(args...)						\
	do {								\
		GAND_SYSLOG(LOG_CRIT, GAND_LOG_XPRE "CRITICAL " args);	\
		GAND_DEBUG("CRITICAL " args);				\
	} while (0)
#define GAND_NOTI_LOG(args...)						\
	do {								\
		GAND_SYSLOG(LOG_NOTICE, GAND_LOG_XPRE "NOTICE " args);	\
		GAND_DEBUG("NOTICE " args);				\
	} while (0)


static inline void
gand_openlog(void)
{
	openlog("gandalfd", GAND_LOG_FLAGS, GAND_FACILITY);
	return;
}

static inline void
gand_closelog(void)
{
	closelog();
	return;
}

#endif	/* INCLUDED_logger_h_ */

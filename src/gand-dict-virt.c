/*** gand-dict-virt.c -- dict reading from VOS triplestore
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#if !defined USE_VIRTUOSO
# error virtuoso triplestore backend not available
#endif	/* !USE_VIRTUOSO */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <sql.h>
#include <sqlext.h>
#if defined HAVE_IODBC
# include <iodbcext.h>
#endif	/* HAVE_IODBC */
#include "gand-dict.h"
#include "nifty.h"
#include "logger.h"

static SQLHANDLE henv = SQL_NULL_HANDLE;
static SQLHANDLE hdbc = SQL_NULL_HANDLE;
static SQLHANDLE stmt = SQL_NULL_HANDLE;

static SQLCHAR rdsn[1024U];
static SQLSMALLINT zdsn;

static char qbuf[1024U];

static int init_odbc(const char *conn);
static int fini_odbc(void);
static int odbc_error(const char *where);

static size_t
xstrlcpy(SQLCHAR *restrict dst, const char *src, size_t dsz)
{
	size_t ssz = strlen(src);
	if (ssz > dsz) {
		ssz = dsz - 1U;
	}
	memcpy(dst, src, ssz);
	dst[ssz] = '\0';
	return ssz;
}


static int
init_odbc(const char *conn)
{
	SQLRETURN rc;
	size_t ndsn;

	/* allocate environment handle */
	rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
	if (!SQL_SUCCEEDED(rc)) {
		goto error;
	}

	/* set ODBC version environment attribute */
	rc = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
	if (!SQL_SUCCEEDED(rc)) {
		goto error;
	}

	/* allocate connection handle */
	rc = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
	if (!SQL_SUCCEEDED(rc)) {
		goto error;
	}

	/* prep connection string */
	ndsn = xstrlcpy(rdsn, conn, sizeof(rdsn));
	/* actually connect to data source */
	rc = SQLDriverConnect(hdbc, SQL_NULL_HANDLE,
			      /*in*/rdsn, ndsn,
			      /*out*/rdsn, sizeof(rdsn), &zdsn,
			      SQL_DRIVER_COMPLETE_REQUIRED);
	if (!SQL_SUCCEEDED(rc)) {
		goto error;
	}

	/* allocate statement handle */
	rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt);
	if (!SQL_SUCCEEDED(rc)) {
		goto error;
	}

	/* success */
	return 0;

error:
	/* failure */
	odbc_error("init_odbc()");
	fini_odbc();
	return -1;
}

static int
fini_odbc(void)
{
	if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}
	stmt = SQL_NULL_HANDLE;

	if (hdbc) {
		(void)SQLDisconnect(hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	}
	hdbc = SQL_NULL_HANDLE;

	if (henv) {
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}
	henv = SQL_NULL_HANDLE;
	return 0;
}

static int
odbc_error(const char *where)
{
	unsigned char buf[256U];
	unsigned char sta[16U];

	/* statement errors */
	while (SQLError(henv, hdbc, stmt, sta, NULL,
			buf, sizeof(buf), NULL) == SQL_SUCCESS) {
		GAND_ERR_LOG("STMT: %s || %s, SQLSTATE=%s", where, buf, sta);
        }

	/* connection errors */
	while (SQLError(henv, hdbc, SQL_NULL_HSTMT, sta, NULL,
			buf, sizeof(buf), NULL) == SQL_SUCCESS) {
		GAND_ERR_LOG("CONN:%s || %s, SQLSTATE=%s\n", where, buf, sta);
        }

	/* environment errors */
	while (SQLError(henv, SQL_NULL_HDBC, SQL_NULL_HSTMT, sta, NULL,
			buf, sizeof(buf), NULL) == SQL_SUCCESS) {
		GAND_ERR_LOG("XENV:%s || %s, SQLSTATE=%s\n", where, buf, sta);
        }
	return -1;
}

static int
odbc_exec(char *qry, size_t qrz)
{
	int rc;

	if ((rc = SQLExecDirect(stmt, (SQLCHAR*)qry, qrz)) != SQL_SUCCESS) {
		odbc_error ("odbc_exec()");
		if (rc != SQL_SUCCESS_WITH_INFO) {
			return -1;
		}
        }
	return 0;
}


dict_t
open_dict(const char *fn, int UNUSED(oflags))
{
	if (init_odbc(fn) < 0) {
		return NULL;
	}
	return stmt;
}

void
close_dict(dict_t UNUSED(d))
{
	(void)fini_odbc();
	return;
}

dict_oid_t
dict_get_sym(dict_t d, const char *sym)
{
/* resolve SYM to RID */
	size_t n;
	SQLRETURN rc;
	SQLLEN rz = 0U;
	dict_oid_t rid;

	n = snprintf(qbuf, sizeof(qbuf), "\
SPARQL \
PREFIX gas: <http://schema.ga-group.nl/symbology#> \
SELECT ?rid FROM <http://data.ga-group.nl/rolf/> WHERE {\
	<http://data.ga-group.nl/rolf/%s> gas:rolfid ?rid .\
}", sym);

	if (UNLIKELY(odbc_exec(qbuf, n) < 0)) {
		return NUL_OID;
	}
	/* otherwise snarf first match */
	if ((rc = SQLFetch(d)) == SQL_NO_DATA_FOUND) {
                return NUL_OID;
	} else if (!SQL_SUCCEEDED(rc)) {
		odbc_error("SQLFetch()");
		return NUL_OID;
	}
	/* get actual data */
	rc = SQLGetData(d, 1, SQL_C_CHAR, qbuf, sizeof(qbuf), &rz);
	if (!SQL_SUCCEEDED(rc)) {
		odbc_error("SQLGetData()");
		return NUL_OID;
	} else if (rz == SQL_NULL_DATA) {
		return NUL_OID;
	}
	/* just try and interpret as number */
	GAND_INFO_LOG("got rz %d\n", rz);
	rid = strtoul((const char*)qbuf, NULL, 10);
	return rid;
}

dict_oid_t
dict_put_sym(dict_t d, const char *sym, dict_oid_t sid)
{
/* assign SID to SYM */
	return NUL_OID;
}

dict_oid_t
dict_next_oid(dict_t d)
{
	return NUL_OID;
}

dict_oid_t
dict_set_next_oid(dict_t d, dict_oid_t oid)
{
	return NUL_OID;
}


/* iterators */
dict_si_t
dict_sym_iter(dict_t d)
{
/* uses static state */
	return (dict_si_t){};
}

dict_si_t
dict_src_iter(dict_t d, const char *src)
{
/* uses static state */
	return (dict_si_t){};
}

/* gand-dict-virt.c ends here */

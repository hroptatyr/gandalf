/*** gand_get_series.c -- obtain time series from gandalf server
 *
 * Copyright (C) 2011-2013 Sebastian Freundt
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* matlab stuff */
#include <mex.h>
/* our stuff */
#include "gandapi.h"
#include "gand_handle.h"
#include "intern.h"
#include "nifty.h"

/* see gand_get_series.m for details */
#define assert(args...)

typedef uint32_t daysi_t;

struct __recv_clo_s {
	obarray_t obsym;
	obarray_t obfld;

	/* alloc'd size of r's arrays */
	size_t z;

	/* filled by qcb */
	/* obint of the first symbol */
	obint_t osym;
	/* actual size of d,f,v */
	size_t n;
	/* vector of dates */
	double *d;
	/* vector of indices into fields obarray */
	double *f;
	/* vector of values */
	double *v;

	/* obint raw data */
	obarray_t obraw;
	double *r;
};


/* date helpers */
#define BASE_YEAR	(1917)
#define TO_BASE(x)	((x) - BASE_YEAR)
#define TO_YEAR(x)	((x) + BASE_YEAR)

static uint16_t dm[] = {
/* this is \sum ml, first element is a bit set of leap days to add */
	0xfff8, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

static daysi_t
idate_to_daysi(idate_t dt)
{
/* compute days since 1917-01-01 (Mon),
 * if year slot is absent in D compute the day in the year of D instead. */
	int y = dt / 10000;
	int m = (dt / 100) % 100;
	int d = dt % 100;
	daysi_t res = dm[m] + d;
	int dy = (y - BASE_YEAR);

	res += dy * 365 + dy / 4;
	if (UNLIKELY(dy % 4 == 3)) {
		res += (dm[0] >> (m)) & 1;
	}
	return res + 700170;
}


#define RESIZE_STEP	(1024)

static void
check_resize(struct __recv_clo_s *st)
{
	/* check for resize */
	const size_t n = st->n;

	if (n < st->z) {
		return;
	}
	/* yep, resize */
	st->z += RESIZE_STEP;

	st->d = realloc(st->d, st->z * sizeof(double));
	memset(st->d + n, -1, RESIZE_STEP * sizeof(double));

	st->f = realloc(st->f, st->z * sizeof(double));
	memset(st->f + n, -1, RESIZE_STEP * sizeof(double));

	st->v = realloc(st->v, st->z * sizeof(double));
	memset(st->v + n, -1, RESIZE_STEP * sizeof(double));

	st->r = realloc(st->r, st->z * sizeof(obint_t));
	memset(st->r + n, -1, RESIZE_STEP * sizeof(obint_t));
	return;
}

static int
qcb(gand_res_t res, void *clo)
{
	struct __recv_clo_s *st = clo;

	if (UNLIKELY(!st->osym)) {
		const size_t ssz = strlen(res->symbol);
		st->osym = intern(st->obsym, res->symbol, ssz);
	}

	check_resize(st);
	st->d[st->n] = idate_to_daysi(res->date);
	with (const char *vrb = res->valflav) {
		size_t z = strlen(res->valflav);

		st->f[st->n] = (double)intern(st->obfld, vrb, z);
	}
	with (const char *val = res->strval) {
		char *on;

		/* try and convert to numeric value */
		if (st->v[st->n] = strtod(val, &on), on == val && *on) {
			size_t z = strlen(val);

			/* bang into r */
			st->r[st->n] = (double)intern(st->obraw, val, z);
		}
	}
	/* more yum please */
	st->n++;
	return 0;
}


void
mexFunction(int UNUSED(nlhs), mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	static const char *_slots[] = {
		"syms", "data", "flds", "raws",
	};
	/* state for retrieval */
	struct __recv_clo_s rst = {.z = 0U};
	const char *srv;
	size_t zrv;
	gand_ctx_t hdl;
	mxArray *slots[countof(_slots)];
	size_t nsym;

	if (nrhs <= 0) {
		mexErrMsgTxt("invalid usage, see `help gand_get_series'");
		return;
	} else if ((hdl = gmx_get_handle(prhs[0])) == NULL) {
		mexErrMsgTxt("gandalf handle seems buggered");
		return;
	} else if ((srv = gand_service(hdl)) == NULL || !(zrv = strlen(srv))) {
		mexErrMsgTxt("gandalf service seems buggered");
		return;
	} else if (nrhs == 1) {
		mexErrMsgTxt("no symbol given\n");
		return;
	}

	/* prep sym obarray */
	rst.obsym = make_obarray();
	for (size_t i = 0U, _nsym = nrhs - 1; i < _nsym; i++) {
		char *sym;

		if (LIKELY(mxIsChar(prhs[i + 1U]) &&
			   (sym = mxArrayToString(prhs[i + 1U])) != NULL)) {
			size_t ssz = strlen(sym);

			if (LIKELY(ssz > 0U)) {
				(void)intern(rst.obsym, sym, ssz);
			}
			mxFree(sym);
		}
	}

	/* now create the result */
	*plhs = mxCreateStructMatrix(1, 1, countof(_slots), _slots);
	/* and an initial cell for the syms slot */
	nsym = ninterns(rst.obsym);
	slots[0U] = mxCreateCellMatrix(nsym, 1U);
	slots[1U] = mxCreateCellMatrix(nsym, 1U);
	slots[2U] = mxCreateCellMatrix(nsym, 1U);
	slots[3U] = mxCreateCellMatrix(nsym, 1U);
	mxSetField(*plhs, 0, _slots[0U], slots[0U]);
	mxSetField(*plhs, 0, _slots[1U], slots[1U]);
	mxSetField(*plhs, 0, _slots[2U], slots[2U]);
	mxSetField(*plhs, 0, _slots[3U], slots[3U]);

	/* generate obarrays for fields and raw values */
	rst.obfld = make_obarray();
	rst.obraw = make_obarray();
	for (obint_t i = 1U; i <= nsym;
	     i++, clear_interns(rst.obfld), clear_interns(rst.obraw)) {
		const char *sym = obint_name(rst.obsym, i);
		size_t nfld;
		mxArray *xfld;
		size_t nraw;
		mxArray *xraw;
		size_t n;

		/* rinse */
		rst.n = 0U;
		rst.osym = (obint_t)0;
		gand_get_series(hdl, sym, qcb, &rst);

		if (UNLIKELY(!rst.osym)) {
			continue;
		}
		/* otherwise copy stuff into result struct,
		 * use real sym string though */
		sym = obint_name(rst.obsym, rst.osym);
		mxSetCell(slots[0U], i - 1U, mxCreateString(sym));

		nfld = ninterns(rst.obfld);
		xfld = mxCreateCellMatrix(nfld, 1U);
		mxSetCell(slots[2U], i - 1U, xfld);
		for (obint_t j = 1U; j <= nfld; j++) {
			const char *fld = obint_name(rst.obfld, j);
			mxSetCell(xfld, j - 1U, mxCreateString(fld));
		}

		nraw = ninterns(rst.obraw);
		xraw = mxCreateCellMatrix(nraw, 1U);
		mxSetCell(slots[3U], i - 1U, xraw);
		for (obint_t j = 1U; j <= nraw; j++) {
			const char *raw = obint_name(rst.obraw, j);
			mxSetCell(xraw, j - 1U, mxCreateString(raw));
		}

		n = rst.n;
		xfld = mxCreateDoubleMatrix(n, 4U, mxREAL);
		mxSetCell(slots[1U], i - 1U, xfld);
		with (double *restrict p = mxGetPr(xfld)) {
			memcpy(p + 0U * n, rst.d, n * sizeof(double));
			memcpy(p + 1U * n, rst.f, n * sizeof(double));
			memcpy(p + 2U * n, rst.v, n * sizeof(double));
			memcpy(p + 3U * n, rst.r, n * sizeof(double));
		}
	}

	if (LIKELY(rst.z > 0U)) {
		free(rst.d);
		free(rst.f);
		free(rst.v);
		free(rst.r);
	}

	free_obarray(rst.obfld);
	free_obarray(rst.obsym);
	free_obarray(rst.obraw);
	return;
}

/* gand_get_series.c ends here */

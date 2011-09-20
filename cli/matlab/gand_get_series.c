/*** gand_get_series.c -- obtain time series from gandalf server
 *
 * Copyright (C) 2011 Sebastian Freundt
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* our stuff */
#include "gandapi.h"
/* matlab stuff */
#include "mex.h"

/* see gand_get_series.m for details */

#define countof(x)	(sizeof(x) / sizeof(*x))
#define assert(args...)
#define LIKELY(x)	(x)
#define UNLIKELY(x)	(x)

typedef uint32_t daysi_t;

struct __recv_st_s {
	idate_t ldat;
	uint32_t lidx;
	double *d;
	double *v;
	char **vf;
	char **res_vf;
	uint32_t nvf;
	uint32_t ncol;
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

static int
find_valflav(const char *vf, struct __recv_st_s *st)
{
	size_t vflen = strlen(vf);

	for (size_t i = 0; i < st->nvf; i++) {
		const char *p = st->vf[i];
		const char *tmp;

		if ((tmp = strstr(p, vf)) &&
		    (tmp == p || tmp[-1] == '/') &&
		    (tmp[vflen] == '\0' || tmp[vflen] == '/')) {
			return i;
		}
	}
	return st->nvf ? -1 : 0;
}

static void
check_resize(struct __recv_st_s *st)
{
	/* check for resize */
	if (st->lidx % RESIZE_STEP) {
		return;
	}
	/* yep, resize */
	st->d = mxRealloc(st->d, (st->lidx + RESIZE_STEP) * sizeof(double));
	memset(st->d + st->lidx, -1, RESIZE_STEP * sizeof(double));
	if (st->ncol) {
		size_t row_sz = st->ncol * sizeof(double);
		size_t new_sz = (st->lidx + RESIZE_STEP) * row_sz;

		st->v = mxRealloc(st->v, new_sz);
		memset(st->v + st->lidx * st->ncol, -1, RESIZE_STEP * row_sz);
	}
	return;
}

static int
qcb(gand_res_t res, void *clo)
{
	struct __recv_st_s *st = clo;
	int this_vf;

	if (res->date > st->ldat) {
		st->lidx++;
		check_resize(st);
		/* set the date */
		st->d[st->lidx] = idate_to_daysi(st->ldat = res->date);
	}

	if (st->v &&
	    (this_vf = find_valflav(res->valflav, st)) >= 0) {
		/* also fill the second matrix */
		st->v[st->lidx * st->ncol + this_vf] = res->value;
		if (st->res_vf && st->res_vf[this_vf] == NULL) {
			st->res_vf[this_vf] = strdup(res->valflav);
		}
	}
	return 0;
}


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	gand_ctx_t gctx;
	char *srv;
	char *sym;
	/* state for retrieval */
	struct __recv_st_s rst = {0};

	if (nrhs == 0 ||
	    (srv = mxArrayToString(prhs[0])) == NULL) {
		mexErrMsgTxt("service string not defined\n");
	} else if (nrhs == 1 ||
		   (sym = mxArrayToString(prhs[1])) == NULL) {
		mxFree(srv);
		mexErrMsgTxt("symbol not given\n");
	} else if (nlhs == 0) {
		mxFree(srv);
		mxFree(sym);
		return;
	} else if (nrhs > 2) {
		rst.vf = mxCalloc(rst.nvf = nrhs - 2, sizeof(*rst.vf));
	}

	for (int i = 0; i < nrhs - 2; i++) {
		char *vf = mxArrayToString(prhs[i + 2]);
		rst.vf[i] = vf;
	}

	/* set up the closure */
	if (nlhs > 1) {
		rst.ncol = nrhs - 2 ?: 1;
	}
	if (nlhs > 2) {
		rst.res_vf = mxCalloc(rst.nvf, sizeof(*rst.res_vf));
	}
	/* start with a negative index */
	rst.lidx = -1;

	/* open the gandalf handle */
	gctx = gand_open(srv, /*timeout*/2500);
	gand_get_series(gctx, sym, rst.vf, rst.nvf, qcb, &rst);
	/* and fuck off again */
	gand_close(gctx);

	/* now reset the matrices to their true dimensions */
	rst.lidx = rst.ldat > 0 ? rst.lidx + 1 : 0;

	plhs[0] = mxCreateDoubleMatrix(0, 1, mxREAL);
	mxSetPr(plhs[0], rst.d);
	mxSetM(plhs[0], rst.lidx);
	if (nlhs > 1 && rst.ncol == 1) {
		plhs[1] = mxCreateDoubleMatrix(0, rst.ncol, mxREAL);
		mxSetPr(plhs[1], rst.v);
		mxSetM(plhs[1], rst.lidx);
	} else if (nlhs > 1) {
		/* since matlab is col oriented, transpose the matrix */
		double *pr;
		plhs[1] = mxCreateDoubleMatrix(rst.lidx, rst.ncol, mxREAL);
		pr = mxGetPr(plhs[1]);
		for (size_t c = 0; c < rst.ncol; c++) {
			for (size_t r = 0; r < rst.lidx; r++) {
				size_t old_pos = r * rst.ncol + c;
				size_t new_pos = c * rst.lidx + r;
				pr[new_pos] = rst.v[old_pos];
			}
		}
		mxFree(rst.v);
	}
	if (nlhs > 2 && rst.res_vf) {
		/* also bang the fields */
		mwSize dim = rst.nvf;
		mxArray *ca = plhs[2] = mxCreateCellArray(1, &dim);
		for (size_t i = 0; i < rst.nvf; i++) {
			mxArray *s = mxCreateString(rst.res_vf[i]);
			mxSetCell(ca, i, s);
			free(rst.res_vf[i]);
		}
		mxFree(rst.res_vf);
	}
	if (rst.vf) {
		for (size_t i = 0; i < rst.nvf; i++) {
			mxFree(rst.vf[i]);
		}
		mxFree(rst.vf);
	}
	mxFree(srv);
	mxFree(sym);
	return;
}

/* gand_get_series.c ends here */

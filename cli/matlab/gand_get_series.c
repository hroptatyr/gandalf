#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* our stuff */
#include "gandapi.h"
/* matlab stuff */
#include "mex.h"

/**
 * [d, p] = gand_get_series(service, symbol, valflav, ...)
 *          reads a gandalf time series
 *
 * Input:
 * symbol
 * optional: valflav cell array
 *
 * Output:
 * date, price time series, one column per valflav
 *
 **/

#define countof(x)	(sizeof(x) / sizeof(*x))
#define assert(args...)
#define LIKELY(x)	(x)

/* grrrr */
extern void mexFunction(int, mxArray*[], int, const mxArray*[]);

struct __recv_st_s {
	idate_t ldat;
	uint32_t lidx;
	double *d;
	double *v;
	char **vf;
	uint32_t nvf;
	uint32_t ncol;
};


static int
find_valflav(const char *vf, struct __recv_st_s *st)
{

	for (size_t i = 0; i < st->nvf; i++) {
		const char *p = st->vf[i];
		if (strcmp(p, vf) == 0) {
			return i;
		}
	}
	return st->nvf ? -1 : 0;
}

static void
check_resize(struct __recv_st_s *st)
{
	/* check for resize */
	if (st->lidx % 1024) {
		return;
	}
	/* yep, resize */
	st->d = mxRealloc(st->d, (st->lidx + 1024) * sizeof(double));
	if (st->ncol) {
		size_t row_sz = st->ncol * sizeof(double);
		size_t new_sz = (st->lidx + 1024) * row_sz;

		st->v = mxRealloc(st->v, new_sz);
		for (size_t r = st->lidx; r > 0; r--) {
			size_t old_j = (r - 1) * st->ncol;
			size_t new_j = (r + 1024 - 1) * st->ncol;
			memmove(st->v + new_j, st->v + old_j, row_sz);
		}
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
		st->d[st->lidx] = st->ldat = res->date;
	}

	if (st->v &&
	    (this_vf = find_valflav(res->valflav, st)) >= 0) {
		/* also fill the second matrix */
		st->v[st->lidx * st->ncol + this_vf] = res->value;
	}
	return 0;
}


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	gand_ctx_t gctx;
	const char *srv;
	const char *sym;
	/* state for retrieval */
	struct __recv_st_s rst = {0};

	if (nrhs == 0 ||
	    (srv = mxArrayToString(prhs[0])) == NULL) {
		mexErrMsgTxt("service string not defined\n");
	} else if (nrhs == 1 ||
		   (sym = mxArrayToString(prhs[1])) == NULL) {
		mexErrMsgTxt("symbol not given\n");
	} else if (nlhs == 0) {
		return;
	} else if (nrhs > 2) {
		rst.vf = calloc(rst.nvf = nrhs - 2, sizeof(*rst.vf));
	}

	for (size_t i = 0; i < nrhs - 2; i++) {
		const char *vf = mxArrayToString(prhs[i + 2]);
		rst.vf[i] = strdup(vf);
	}

	/* set up the closure */
	if (nlhs > 1) {
		rst.ncol = nrhs - 2 ?: 1;
	}
	/* start with a negative index */
	rst.lidx = -1;

	/* open the gandalf handle */
	gctx = gand_open(srv, /*timeout*/2000);
	gand_get_series(gctx, sym, rst.vf, rst.nvf, qcb, &rst);
	/* and fuck off again */
	gand_close(gctx);

	/* now reset the matrices to their true dimensions */
	rst.lidx = rst.ldat > 0 ? rst.lidx + 1 : 0;

	plhs[0] = mxCreateDoubleMatrix(0, 1, mxREAL);
	mxSetPr(plhs[0], rst.d);
	mxSetM(plhs[0], rst.lidx);
	if (nlhs > 1) {
		plhs[1] = mxCreateDoubleMatrix(0, rst.ncol, mxREAL);
		fprintf(stderr, "%p\n", rst.v);
		mxSetPr(plhs[1], rst.v);
		mxSetM(plhs[1], rst.lidx);
	}
	if (rst.vf) {
		for (size_t i = 0; i < rst.nvf; i++) {
			free(rst.vf[i]);
		}
		free(rst.vf);
	}
	return;
}

/* gand_get_series.c ends here */

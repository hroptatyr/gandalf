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
	mxArray *d;
	mxArray *v;
	const char **vf;
	size_t nvf;
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

int
qcb(gand_res_t res, void *clo)
{
	struct __recv_st_s *st = clo;
	int this_vf;

	if (res->date > st->ldat) {
		if (st->ldat > 0) {
			st->lidx++;
		}
		/* check for resize */
		if (st->lidx % 1024 == 0) {
			/* yep */
			mxSetM(st->d, st->lidx + 1024);
			if (st->v) {
				mxSetM(st->v, st->lidx + 1024);
			}
		}
		/* set the date */
		mxGetPr(st->d)[st->lidx] = st->ldat = res->date;
	}

	if (st->v &&
	    (this_vf = find_valflav(res->valflav, st)) >= 0) {
		/* also fill the second matrix */
		mxGetPr(st->d)[st->lidx * mxGetN(st->d) + this_vf] = res->value;
	}
	return 0;
}


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	gand_ctx_t gctx;
	const char *srv;
	const char *sym;
	const char *vfs[nrhs - 2];
	/* state for retrieval */
	struct __recv_st_s recv_st = {0};

	if (nrhs == 0 ||
	    (srv = mxArrayToString(prhs[0])) == NULL) {
		mexErrMsgTxt("service string not defined\n");
	} else if (nrhs == 1 ||
		   (sym = mxArrayToString(prhs[1])) == NULL) {
		mexErrMsgTxt("symbol not given\n");
	} else if (nrhs > 2) {
		recv_st.vf = vfs;
		recv_st.nvf = nrhs - 2;
	}

	if (nlhs == 0) {
		return;
	}
	for (size_t i = 0; i < nrhs - 2; i++) {
		const char *vf = mxArrayToString(prhs[i + 2]);
		vfs[i] = vf;
	}

	/* set up the closure */
	recv_st.d = plhs[0] = mxCreateDoubleMatrix(0, 1, mxREAL);
	if (nlhs > 1) {
		int ncols = nrhs - 2 ?: 1;
		recv_st.v = plhs[1] = mxCreateDoubleMatrix(0, ncols, mxREAL);
	}

	/* open the gandalf handle */
	gctx = gand_open(srv, /*timeout*/2000);
	gand_get_series(gctx, sym, vfs, nrhs - 2, qcb, &recv_st);
	/* and fuck off again */
	gand_close(gctx);

	/* now reset the matrices to their true dimensions */
	recv_st.lidx = recv_st.ldat > 0 ? recv_st.lidx + 1 : 0;

	mxSetM(plhs[0], recv_st.lidx);
	if (nlhs > 1) {
		mxSetM(plhs[1], recv_st.lidx);
	}
	return;
}

/* gand_get_series.c ends here */

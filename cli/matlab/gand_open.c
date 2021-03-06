/*** gand_open.c -- gandalf handle acquisition
 *
 * Copyright (C) 2013 Sebastian Freundt
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
#include <stdint.h>
/* matlab stuff */
#include <mex.h>
/* our stuff */
#include "gandapi.h"
#include "gand_handle.h"

static char*
snarf_fname(const mxArray *fn)
{
	if (UNLIKELY(!mxIsChar(fn))) {
		/* big bugger */
		return NULL;
	}
	return mxArrayToString(fn);
}


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	gand_ctx_t hdl;
	char *fn;

	if (nrhs != 1 || nlhs != 1) {
		mexErrMsgTxt("invalid usage, see `help gand_open'");
		return;
	} else if ((fn = snarf_fname(prhs[0])) == NULL) {
		mexErrMsgTxt("cannot determine file name to open");
		return;
	} else if ((hdl = gand_open(fn, /*:timeout */2500)) == NULL) {
		mexWarnMsgTxt("cannot establish connection to gandalf server");
	} else {
		/* otherwise just assign the handle */
		plhs[0] = make_gmx_handle();
		gmx_put_handle(plhs[0], hdl);
	}
	/* free file name */
	mxFree(fn);
	return;
}

/* gand_open.c ends here */

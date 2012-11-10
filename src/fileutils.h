/*** fileutils.h -- mapping files and stuff
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
#if !defined INCLUDED_fileutils_h_
#define INCLUDED_fileutils_h_

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "nifty.h"

/* mmap buffers, memory and file based */
struct mmmb_s {
	char *buf;
	/* real size */
	size_t bsz;
	/* alloc size */
	size_t all;
};

struct mmfb_s {
	struct mmmb_s m;
	/* file desc */
	int fd;
};

static void
munmap_all(struct mmfb_s *mf)
{
	if (mf->m.all > 0UL && mf->m.buf != NULL && mf->m.buf != MAP_FAILED) {
		munmap(mf->m.buf, mf->m.all);
	}
	if (mf->fd >= 0) {
		close(mf->fd);
	}
	/* reset values */
	mf->m.buf = NULL;
	mf->m.bsz = mf->m.all = 0UL;
	mf->fd = -1;
	return;
}

static ssize_t
mmap_whole_file(struct mmfb_s *mf, const char *f)
{
	size_t fsz = 0;
	struct stat st = {0};

	if (UNLIKELY(stat(f, &st) < 0)) {
		return -1;
	} else if (UNLIKELY((fsz = st.st_size) == 0)) {
		return -1;
	}

	if (UNLIKELY((mf->fd = open(f, O_RDONLY)) < 0)) {
		return -1;
	}

	/* mmap the file */
	mf->m.buf = mmap(NULL, fsz, PROT_READ, MAP_SHARED, mf->fd, 0);
	if (UNLIKELY(mf->m.buf == MAP_FAILED)) {
		munmap_all(mf);
		return -1;
	}
	return mf->m.all = mf->m.bsz = fsz;
}

#endif	/* INCLUDED_fileutils_h_ */

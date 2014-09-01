/*** clidalf.c -- just a command line version of what the server should be */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include "gand-dict.h"
#include "nifty.h"
#include "fops.h"

typedef struct {
	const char *s;
	size_t z;
} word_t;

struct rln_s {
	word_t sym;
	word_t dat;
	word_t vrb;
	word_t val;
};

static char *trolfdir = "/var/scratch/freundt/trolf";
static size_t trolfdiz = sizeof("/var/scratch/freundt/trolf") - 1U/*\nul*/;
static dict_t gsymdb;

#if !defined PATH_MAX
# define PATH_MAX	(256U)
#endif	/* PATH_MAX */


static __attribute__((format(printf, 1, 2))) void
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static const char*
make_lateglu_name(dict_oid_t rolf_id)
{
	static const char glud[] = "show_lateglu/";
	static char f[PATH_MAX];
	size_t idx;

	/* construct the path */
	memcpy(f, trolfdir, (idx = trolfdiz));
	if (f[idx - 1] != '/') {
		f[idx++] = '/';
	}
	memcpy(f + idx, glud, sizeof(glud) - 1);
	idx += sizeof(glud) - 1;
	snprintf(
		f + idx, PATH_MAX - idx,
		/* this is the split version */
		"%04u/%08u", rolf_id / 10000U, rolf_id);
	return f;
}

static struct rln_s
snarf_rln(const char *ln, size_t lz)
{
	struct rln_s r;
	const char *p;

	/* normally first up is the rolf-id, overread him */
	if (UNLIKELY((p = memchr(ln, '\t', lz)) == NULL)) {
		goto b0rk;
	}

	/* snarf sym */
	r.sym.s = ++p;
	/* find separator between symbol and trans-id */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}
	r.sym.z = p++ - r.sym.s;

	/* find separator between trans-id and date stamp */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}

	/* snarf date */
	r.dat.s = ++p;
	/* find separator between date stamp and valflav aka verb */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}
	r.dat.z = p++ - r.dat.s;

	/* snarf valflav aka verb */
	r.vrb.s = p;
	/* find separator between date stamp and valflav aka verb */
	if (UNLIKELY((p = memchr(p, '\t', ln + lz - p)) == NULL)) {
		goto b0rk;
	}
	r.vrb.z = p++ - r.vrb.s;

	/* snarf value */
	r.val.s = p;
	r.val.z = ln + lz - p;

	/* that's all */
	return r;

b0rk:
	return (struct rln_s){NULL};
}

static void
filtshow(const char *data, const size_t dlen)
{
	char buf[4096U];
	size_t tot = 0U;

	/* go through the buffer generate snarf lines */
	for (size_t i = 0U; i < dlen; i++) {
		const char *const bol = data + i;
		const size_t max = dlen - i;
		char *restrict bp = buf + tot;
		const char *eol;
		struct rln_s ln;

		if (UNLIKELY((eol = memchr(bol, '\n', max)) == NULL)) {
			eol = bol + max;
		}
		/* snarf the line, v0 format, zero copy */
		ln = snarf_rln(bol, eol - bol);

		/* apply filters */
		;

		if (UNLIKELY(ln.sym.s == NULL)) {
			continue;
		} else if (UNLIKELY(tot +
				    ln.sym.z + 1U/*\t*/ +
				    ln.dat.z + 1U/*\t*/ +
				    ln.vrb.z + 1U/*\t*/ +
				    ln.val.z + 1U/*\n*/ >= sizeof(buf))) {
			/* flush */
			write(STDOUT_FILENO, buf, tot);
			bp = buf + (tot = 0U);
		}

		/* show results */
		memcpy(bp, ln.sym.s, ln.sym.z);
		bp += ln.sym.z;
		*bp++ = '\t';

		memcpy(bp, ln.dat.s, ln.dat.z);
		bp += ln.dat.z;
		*bp++ = '\t';

		memcpy(bp, ln.vrb.s, ln.vrb.z);
		bp += ln.vrb.z;
		*bp++ = '\t';

		memcpy(bp, ln.val.s, ln.val.z);
		bp += ln.val.z;
		*bp++ = '\n';

		tot += bp - (buf + tot);
		i += eol - bol;
	}
	/* flush again */
	write(STDOUT_FILENO, buf, tot);
	return;
}


#include "clidalf.yucc"

static int
cmd_show(const struct yuck_cmd_show_s argi[static 1U])
{
	for (size_t i = 0U; i < argi->symbol_nargs; i++) {
		const char *sym = argi->symbol_args[i];
		dict_oid_t rid;
		const char *fn;
		gandfn_t fb;

		if (!(rid = dict_get_sym(gsymdb, sym))) {
			errno = 0;
			error("symbol not found: %s\n", sym);
			continue;
		} else if (UNLIKELY((fn = make_lateglu_name(rid)) == NULL)) {
			error("\
Error: cannot construct lateglu file name: %s  (%08u)\n", sym, rid);
			continue;
		} else if (UNLIKELY((fb = mmap_fn(fn, O_RDONLY)).fd < 0)) {
			error("\
Error: cannot access lateglu file: %s  (%08u)\n", sym, rid);
			continue;
		}

		/* filter and show results */
		filtshow(fb.fb.d, fb.fb.z);

		/* and close the bugger again */
		munmap_fn(fb);
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	/* args */
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out0;
	}

	/* get trolfdir or use default */
	if (argi->trolfdir_arg) {
		trolfdir = argi->trolfdir_arg;
		trolfdiz = strlen(trolfdir);
	}

	if ((gsymdb = open_dict("gand_idx2sym.tcb", O_RDONLY)) == NULL) {
		error("cannot open symbol index file");
		rc = 1;
		goto out0;
	}


	switch (argi->cmd) {
	default:
		break;
	case CLIDALF_CMD_SHOW:
		rc = cmd_show((const void*)argi);
		break;
	}

	close_dict(gsymdb);
out0:
	yuck_free(argi);
	return rc;
}

/* clidalf.c ends here */

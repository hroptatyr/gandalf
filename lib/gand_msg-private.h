#if !defined INCLUDED_gand_msg_private_h_
#define INCLUDED_gand_msg_private_h_

#include <sys/mman.h>
#include "gandalf.h"

static inline void
resize_mall(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	if (cnt == 0) {
		size_t new = inc * blksz;
		*ptr = malloc(new);

	} else if (cnt % inc == 0) {
		/* resize */
		size_t new = (cnt + inc) * blksz;
		*ptr = realloc(*ptr, new);
	}
	return;
}

static inline void
unsize_mall(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	size_t sz;

	if (*ptr == NULL || cnt == 0) {
		return;
	}

	free(*ptr);
	*ptr = NULL;
	return;
}

#define PROT_MEM		(PROT_READ | PROT_WRITE)
#define MAP_MEM			(MAP_PRIVATE | MAP_ANONYMOUS)

static inline void
resize_mmap(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	if (cnt == 0) {
		size_t new = inc * blksz;
		*ptr = mmap(NULL, new, PROT_MEM, MAP_MEM, 0, 0);

	} else if (cnt % inc == 0) {
		/* resize */
		size_t old = cnt * blksz;
		size_t new = (cnt + inc) * blksz;
		*ptr = mremap(*ptr, old, new, MREMAP_MAYMOVE);
	}
	return;
}

static inline void
unsize_mmap(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	size_t sz;

	if (*ptr == NULL || cnt == 0) {
		return;
	}

	sz = (cnt / inc + 1) * inc * blksz;
	fprintf(stderr, "munmapping %p %zu\n", *ptr, sz);
	munmap(*ptr, sz);
	*ptr = NULL;
	return;
}

static idate_t
__to_idate(const char *dstr)
{
	char *p;
	uint32_t y = strtoul(dstr, &p, 10);
	uint32_t m = strtoul(p + 1, &p, 10);
	uint32_t d = strtoul(p + 1, &p, 10);
	return y * 10000 + m * 100 + d;
}


/* parser functions */
extern int __parse(gand_msg_t msg, const char *s, size_t l);

static inline void
resize_rolf_objs(gand_msg_t msg)
{
	resize_mall(
		(void**)&msg->rolf_objs,
		msg->nrolf_objs,
		sizeof(*msg->rolf_objs),
		GAND_MSG_ROLF_OBJS_INC);
	return;
}

static inline void
unsize_rolf_objs(gand_msg_t msg)
{
	unsize_mall(
		(void**)&msg->rolf_objs,
		msg->nrolf_objs,
		sizeof(*msg->rolf_objs),
		GAND_MSG_ROLF_OBJS_INC);
	return;
}

static inline void
resize_date_rngs(gand_msg_t msg)
{
	resize_mall(
		(void**)&msg->date_rngs,
		msg->ndate_rngs,
		sizeof(*msg->date_rngs),
		GAND_MSG_DATE_RNGS_INC);
	return;
}

static inline void
unsize_date_rngs(gand_msg_t msg)
{
	unsize_mall(
		(void**)&msg->date_rngs,
		msg->ndate_rngs,
		sizeof(*msg->date_rngs),
		GAND_MSG_DATE_RNGS_INC);
	return;
}

#endif	/* INCLUDED_gand_msg_private_h_ */

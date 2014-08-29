#if !defined INCLUDED_gandapi_h_
#define INCLUDED_gandapi_h_

#include <stdint.h>
#include <unistd.h>

typedef struct gand_ctx_s *gand_ctx_t;
typedef struct gand_res_s *gand_res_t;
typedef uint32_t idate_t;

/* gandalf response */
struct gand_res_s {
	const char *symbol;
	idate_t date;
	const char *valflav;
	const char *strval;
};

/**
 * Open a gandalf connection and return a handle.
 *
 * \param SRV service string, either HOST:PORT or PATH or unix:PATH
 *   for ipv6 addresses use [ADDR]:PORT */
extern gand_ctx_t gand_open(const char *srv, int timeout);

/**
 * Close a gandalf connection, and free associated resources. */
extern void gand_close(gand_ctx_t);

/**
 * Return the service string as handed in to gand_open(). */
extern const char *gand_service(gand_ctx_t);

/**
 * Query the gandalf server. */
extern int
gand_get_series(
	gand_ctx_t,
	const char *qry,
	int(*)(gand_res_t, void *closure), void *closure);

#endif	/* INCLUDED_gandapi_h_ */

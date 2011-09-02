### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

SUBDIRS = . matlab

noinst_PROGRAMS = gandqry
noinst_HEADERS =

noinst_LTLIBRARIES = libgandapi.la
libgandapi_la_SOURCES = gandapi.c gandapi.h

gandqry_LDADD = libgandapi.la

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

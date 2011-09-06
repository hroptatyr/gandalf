### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

EXTRA_DIST = $(BUILT_SOURCES)
BUILT_SOURCES =

SUBDIRS = .
if BUILD_MATCLI
SUBDIRS += matlab
endif

if BUILD_CLIAPPS
noinst_LTLIBRARIES = libgandapi.la
libgandapi_la_SOURCES = gandapi.c gandapi.h

noinst_PROGRAMS = gandqry
gandqry_LDADD = libgandapi.la
BUILT_SOURCES += gandqry-clo.c gandqry-clo.h
endif

## ggo rule
%.c %.h: %.ggo
	gengetopt -l -i $< -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

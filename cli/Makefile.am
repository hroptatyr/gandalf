### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

EXTRA_DIST = $(BUILT_SOURCES)
BUILT_SOURCES =
SUFFIXES =

SUBDIRS = .
if BUILD_MATCLI
SUBDIRS += matlab
endif  BUILD_MATCLI

if BUILD_CLIAPPS
noinst_LTLIBRARIES = libgandapi.la
libgandapi_la_SOURCES = gandapi.c gandapi.h
libgandapi_la_CPPFLAGS = $(AM_CPPFLAGS)
libgandapi_la_CPPFLAGS += $(curl_CFLAGS)
libgandapi_la_LDFLAGS = $(AM_LDFLAGS)
libgandapi_la_LDFLAGS += $(curl_LIBS)

noinst_PROGRAMS = gandqry
gandqry_SOURCES = gandqry.c
gandqry_SOURCES += gandqry.yuck
gandqry_LDADD = libgandapi.la
BUILT_SOURCES += gandqry.yucc
endif

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_CPPFLAGS += -DHAVE_VERSION_H
AM_LDFLAGS =

bin_PROGRAMS =
sbin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
noinst_LTLIBRARIES =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =
SUFFIXES =

CLEANFILES += version.c
EXTRA_DIST += version.c.in
noinst_HEADERS += version.h


noinst_LTLIBRARIES += libgand.la
libgand_la_SOURCES =
libgand_la_SOURCES += nifty.h
libgand_la_SOURCES += version.h version.c
libgand_la_SOURCES += fops.c fops.h
libgand_la_SOURCES += logger.c logger.h
libgand_la_SOURCES += configger.c configger.h
if HAVE_LUA
libgand_la_SOURCES += lua-config.c lua-config.h
libgand_la_SOURCES += gand-cfg.c gand-cfg.h
cfg_CFLAGS = -DUSE_LUA $(lua_CFLAGS)
cfg_LIBS = $(lua_LIBS)
endif HAVE_LUA
libgand_la_SOURCES += gand-dict.c gand-dict.h
libgand_la_CPPFLAGS = $(AM_CPPFLAGS)
libgand_la_CPPFLAGS += -DUSE_REDLAND
libgand_la_CPPFLAGS += $(cfg_CFLAGS)
libgand_la_CPPFLAGS += $(dict_CFLAGS)
libgand_la_LDFLAGS = $(AM_LDFLAGS)
libgand_la_LDFLAGS += $(cfg_LIBS) $(dict_LIBS)
dict_CFLAGS = $(tokyocabinet_CFLAGS)
dict_CFLAGS += $(shell pkg-config --cflags rasqal)
dict_CFLAGS += $(shell pkg-config --cflags raptor2)
dict_CFLAGS += $(shell pkg-config --cflags redland)
dict_LIBS = $(tokyocabinet_LIBS)
dict_LIBS += $(shell pkg-config --libs rasqal)
dict_LIBS += $(shell pkg-config --libs raptor2)
dict_LIBS += $(shell pkg-config --libs redland)

## our own take on a http server
noinst_LTLIBRARIES += libbeef.la
libbeef_la_SOURCES =
EXTRA_libbeef_la_SOURCES =
libbeef_la_SOURCES += httpd.c httpd.h
libbeef_la_SOURCES += ud-sock.h
EXTRA_libbeef_la_SOURCES += httpd-verb-gp.erf
libbeef_la_CPPFLAGS = $(AM_CPPFLAGS)
libbeef_la_CPPFLAGS += $(libev_CFLAGS)
libbeef_la_LDFLAGS = $(AM_LDFLAGS)
libbeef_la_LDFLAGS += $(libev_LIBS)
if HAVE_ZLIB
libbeef_la_LDFLAGS += -lz
endif  HAVE_ZLIB
BUILT_SOURCES += httpd-verb-gp.c


if BUILD_SERVER
bin_PROGRAMS += gandaux
gandaux_SOURCES = gandaux.c
gandaux_SOURCES += gandaux.yuck
gandaux_SOURCES += nifty.h
gandaux_SOURCES += version.h version.c
gandaux_CPPFLAGS = $(AM_CPPFLAGS)
gandaux_CPPFLAGS += -DHAVE_VERSION_H
gandaux_CPPFLAGS += -DUSE_REDLAND
gandaux_CPPFLAGS += $(dict_CFLAGS)
gandaux_LDFLAGS = $(AM_LDFLAGS)
gandaux_LDFLAGS += $(dict_LIBS)
gandaux_LDADD = libgand.la
endif  BUILD_SERVER
BUILT_SOURCES += gandaux.yucc

if BUILD_SERVER
sbin_PROGRAMS += gandalfd
gandalfd_SOURCES = gandalfd.c
gandalfd_SOURCES += gandalfd.yuck
EXTRA_gandalfd_SOURCES =
gandalfd_CPPFLAGS = $(AM_CPPFLAGS)
gandalfd_CPPFLAGS += -DUSE_REDLAND
gandalfd_CPPFLAGS += $(dict_CFLAGS)
gandalfd_CPPFLAGS += $(cfg_CFLAGS)
gandalfd_CPPFLAGS += $(libev_CFLAGS)
gandalfd_LDFLAGS = $(AM_LDFLAGS)
gandalfd_LDFLAGS += $(dict_LIBS)
gandalfd_LDFLAGS += $(cfg_LIBS)
gandalfd_LDFLAGS += $(libev_LIBS)
gandalfd_LDADD = libgand.la
gandalfd_LDADD += libbeef.la
endif  BUILD_SERVER
BUILT_SOURCES += gandalfd.yucc

noinst_PROGRAMS += clidalf
clidalf_CPPFLAGS = $(AM_CPPFLAGS)
clidalf_CPPFLAGS += $(dict_CFLAGS)
clidalf_LDFLAGS = $(AM_LDFLAGS)
clidalf_LDFLAGS += $(dict_LIBS)
clidalf_LDADD = libgand.la
BUILT_SOURCES += clidalf.yucc


## version rules
version.c: version.c.in $(top_builddir)/.version
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver --ignore-noscm --force -o $@ \
			--use-reference --reference $(top_builddir)/.version $<
## gperf rule
SUFFIXES += .erf
.erf.c:
	$(AM_V_GEN) $(GPERF) -L ANSI-C "$<" --output-file "$@"

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

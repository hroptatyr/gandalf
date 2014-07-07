### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_LDFLAGS =

bin_PROGRAMS =
sbin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =
SUFFIXES =

if BUILD_SERVER
bin_PROGRAMS += gandaux
gandaux_SOURCES = gandaux.c
gandaux_SOURCES += gandaux.yuck
gandaux_SOURCES += nifty.h
gandaux_CPPFLAGS = $(AM_CPPFLAGS)
gandaux_CPPFLAGS += $(tokyocabinet_CFLAGS)
gandaux_LDFLAGS = $(AM_LDFLAGS)
gandaux_LDFLAGS += $(tokyocabinet_LIBS)
endif  BUILD_SERVER
BUILT_SOURCES += gandaux.yucc

if BUILD_SERVER
sbin_PROGRAMS += gandalfd
gandalfd_SOURCES = gandalfd.c
gandalfd_SOURCES += gandalfd.yuck
gandalfd_SOURCES += fops.h
gandalfd_SOURCES += nifty.h
gandalfd_SOURCES += logger.c logger.h
gandalfd_SOURCES += configger.c configger.h
gandalfd_CPPFLAGS = $(AM_CPPFLAGS)
gandalfd_CPPFLAGS += $(tokyocabinet_CFLAGS)
gandalfd_CPPFLAGS += $(onion_CFLAGS)
gandalfd_LDFLAGS = $(AM_LDFLAGS)
gandalfd_LDFLAGS += $(tokyocabinet_LIBS)
gandalfd_LDFLAGS += $(onion_LIBS)
if HAVE_LUA
gandalfd_SOURCES += lua-config.c lua-config.h
gandalfd_CPPFLAGS += -DUSE_LUA $(lua_CFLAGS)
gandalfd_LDFLAGS += $(lua_LIBS)
endif  HAVE_LUA
endif  BUILD_SERVER
BUILT_SOURCES += gandalfd.yucc


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

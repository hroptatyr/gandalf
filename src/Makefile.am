### Makefile.am

AM_CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_LDFLAGS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =

if BUILD_SERVER
pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = gandalf.pc

sbin_PROGRAMS = gandalfd
gandalfd_SOURCES =
EXTRA_gandalfd_SOURCES =
gandalfd_CPPFLAGS = $(AM_CPPFLAGS)
gandalfd_CPPFLAGS += -D_GNU_SOURCE
gandalfd_LDFLAGS = $(AM_LDFLAGS)
gandalfd_LDADD =
gandalfd_SOURCES += gandalfd.ggo
gandalfd_SOURCES += configger.c configger.h
gandalfd_SOURCES += logger.h
gandalfd_SOURCES += trie.c trie.h triedefs.h
gandalfd_CPPFLAGS += -DSTATIC_TRIE_GUTS
EXTRA_gandalfd_SOURCES += darray.c darray.h
EXTRA_gandalfd_SOURCES += tail.c tail.h
gandalfd_SOURCES += slut.c slut.h
gandalfd_SOURCES += slut-trie-glue.h
gandalfd_SOURCES += fileutils.h
gandalfd_SOURCES += gandalfd.c
EXTRA_gandalfd_SOURCES += gq.c gq.h
gandalfd_CPPFLAGS += $(libev_CFLAGS)
gandalfd_LDFLAGS += $(libev_LIBS)
if HAVE_LUA
gandalfd_SOURCES += lua-config.c lua-config.h
gandalfd_CPPFLAGS += -DUSE_LUA $(lua_CFLAGS)
gandalfd_LDFLAGS += $(lua_LIBS)
endif  HAVE_LUA
gandalfd_CPPFLAGS += -I$(top_srcdir)/lib
gandalfd_LDADD += $(top_builddir)/lib/libgandalf.la
BUILT_SOURCES += gandalfd.x gandalfd.xh
endif ## BUILD_SERVER

bin_PROGRAMS = gandalf
gandalf_SOURCES = gandalf.c gandalf.ggo
gandalf_CPPFLAGS = $(AM_CPPFLAGS)
gandalf_CPPFLAGS += $(tokyocabinet_CFLAGS)
gandalf_LDFLAGS = $(AM_LDFLAGS)
gandalf_LDFLAGS += $(tokyocabinet_LIBS)
BUILT_SOURCES += gandalf.x gandalf.xh


## our rule for gengetopt
%.x %.xh: %.ggo
	$(AM_V_GEN) gengetopt -l -c x -H xh -i $< -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

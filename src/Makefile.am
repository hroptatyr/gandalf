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

bin_PROGRAMS += gandalf
gandalf_SOURCES = gandalf.c gandalf.ggo
gandalf_CPPFLAGS = $(AM_CPPFLAGS)
gandalf_CPPFLAGS += $(tokyocabinet_CFLAGS)
gandalf_LDFLAGS = $(AM_LDFLAGS)
gandalf_LDFLAGS += $(tokyocabinet_LIBS)
BUILT_SOURCES += gandalf.x gandalf.xh

noinst_PROGRAMS += gandalfd
gandalfd_SOURCES = gandalfd.c
gandalfd_SOURCES += gandalfd.ggo
gandalfd_SOURCES += fops.h
gandalfd_SOURCES += logger.c logger.h
gandalfd_SOURCES += configger.c configger.h
gandalfd_CPPFLAGS = $(AM_CPPFLAGS)
gandalfd_CPPFLAGS += -I/home/freundt/usr/include
gandalfd_CPPFLAGS += $(tokyocabinet_CFLAGS)
gandalfd_LDFLAGS = $(AM_LDFALGS)
gandalfd_LDFLAGS += -L/home/freundt/usr/lib
gandalfd_LDFLAGS += $(tokyocabinet_LIBS)
gandalfd_LDADD = -lonion_static -lpthread
if HAVE_LUA
gandalfd_SOURCES += lua-config.c lua-config.h
gandalfd_CPPFLAGS += -DUSE_LUA $(lua_CFLAGS)
gandalfd_LDFLAGS += $(lua_LIBS)
endif  HAVE_LUA


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

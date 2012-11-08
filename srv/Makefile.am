### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
CLEANFILES =

if BUILD_SERVER
pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = gandalf.pc

if BUILD_UD_DSO
moddir = $(libdir)/unserding
mod_LTLIBRARIES = dso-gandalf.la
dso_gandalf_la_SOURCES =
EXTRA_dso_gandalf_la_SOURCES =
dso_gandalf_la_LDFLAGS = -module
dso_gandalf_la_LDFLAGS += $(unserding_LDFLAGS) $(LIBXML2_LIBS)
dso_gandalf_la_LIBADD = $(top_builddir)/lib/.libs/libgandalf.a
dso_gandalf_la_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
dso_gandalf_la_CPPFLAGS += -I$(top_srcdir)/lib
dso_gandalf_la_CPPFLAGS += $(LIBXML2_CFLAGS)
dso_gandalf_la_SOURCES += dso-gandalf.c
endif BUILD_UD_DSO
endif ## BUILD_SERVER


# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

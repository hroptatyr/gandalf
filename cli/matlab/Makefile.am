### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =
MOD_LDFLAGS = -module -shared -avoid-version -shrext ".mexa64"

moddir = $(HOME)/.matlab/$(shell basename "$(MATLABROOT)")/toolbox/gandalf
EXTRA_DIST =
mod_LTLIBRARIES =
mod_DATA =

mod_LTLIBRARIES += gand_get_series.la
mod_DATA += gand_get_series.m
gand_get_series_la_SOURCES = gand_get_series.c
gand_get_series_la_SOURCES += gand_handle.h
gand_get_series_la_SOURCES += intern.c intern.h
gand_get_series_la_LDFLAGS = $(AM_LDFLAGS)
gand_get_series_la_LDFLAGS += $(MOD_LDFLAGS)
gand_get_series_la_LDFLAGS += $(XCCLDFLAGS)
gand_get_series_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
gand_get_series_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_get_series_la_CPPFLAGS += $(matlab_CFLAGS)
gand_get_series_la_CPPFLAGS += -I$(top_srcdir)/src
gand_get_series_la_CPPFLAGS += -DENUM_INTERNS
gand_get_series_la_LIBADD = $(top_builddir)/cli/libgandapi.la
EXTRA_DIST += $(mod_DATA)

mod_LTLIBRARIES += gand_open.la
mod_DATA += gand_open.m
gand_open_la_LDFLAGS = $(AM_LDFLAGS)
gand_open_la_LDFLAGS += $(MOD_LDFLAGS)
gand_open_la_LDFLAGS += $(XCCLDFLAGS)
gand_open_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
gand_open_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_open_la_CPPFLAGS += $(matlab_CFLAGS)
gand_open_la_LIBADD = $(top_builddir)/cli/libgandapi.la
gand_open_la_SOURCES = gand_open.c
gand_open_la_SOURCES += gand_handle.h
EXTRA_DIST += $(mod_DATA)

mod_LTLIBRARIES += gand_close.la
mod_DATA += gand_close.m
gand_close_la_LDFLAGS = $(AM_LDFLAGS)
gand_close_la_LDFLAGS += $(MOD_LDFLAGS)
gand_close_la_LDFLAGS += $(XCCLDFLAGS)
gand_close_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
gand_close_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_close_la_CPPFLAGS += $(matlab_CFLAGS)
gand_close_la_LIBADD = $(top_builddir)/cli/libgandapi.la
gand_close_la_SOURCES = gand_close.c
gand_close_la_SOURCES += gand_handle.h
EXTRA_DIST += $(mod_DATA)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =
MOD_LDFLAGS = -module -shared -avoid-version -shrext ".mexa64"

moddir = $(HOME)/.matlab/$(shell basename "$(MATLABROOT)")/toolbox/gandalf
EXTRA_DIST =

mod_LTLIBRARIES = gand_get_series.la
mod_DATA = gand_get_series.m
gand_get_series_la_LDFLAGS = $(AM_LDFLAGS)
gand_get_series_la_LDFLAGS += $(MOD_LDFLAGS)
gand_get_series_la_LDFLAGS += $(XCCLDFLAGS)
gand_get_series_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
gand_get_series_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_get_series_la_CPPFLAGS += $(matlab_CFLAGS)
gand_get_series_la_LIBADD = $(top_builddir)/cli/libgandapi.la
gand_get_series_la_SOURCES = gand_get_series.c
EXTRA_DIST += $(mod_DATA)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

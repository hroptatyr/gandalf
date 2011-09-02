### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

MATLABROOT = /opt/Matlab/R2010b
moddir = $(libdir)/gandalf/matlab

mod_LTLIBRARIES = gand_get_series.la
gand_get_series_la_LDFLAGS = -module -shrext ".mexa64"
gand_get_series_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
gand_get_series_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_get_series_la_CPPFLAGS += -I$(MATLABROOT)/extern/include
gand_get_series_la_LIBADD = $(top_builddir)/cli/libgandapi.la
gand_get_series_la_SOURCES = gand_get_series.c

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

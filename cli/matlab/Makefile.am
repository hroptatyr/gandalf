### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =
MEX_LDFLAGS = -module -shared -avoid-version -shrext ".mex"

mex_LTLIBRARIES =
mex_DATA =
EXTRA_DIST += $(mex_DATA)

if BUILD_MATCLI
mexdir = $(MATLABPATH)/gandalf
octlab_CFLAGS = -DMATLAB_MEX_FILE $(matlab_CFLAGS)
else  !BUILD_MATCLI

## backup plan is to use octave to build the mex file
if BUILD_OCTCLI
mexdir = $(OCTAVEPATH)/gandalf
octlab_CFLAGS = -DOCTAVE_MEX_FILE $(octave_CFLAGS)

else  !BUILD_OCTCLI
## no idea what we're doing here
mexdir = $(pkglibexecdir)

endif  !BUILD_OCTCLI
endif  !BUILD_MATCLI


mex_LTLIBRARIES += gand_get_series.la
mex_DATA += gand_get_series.m
gand_get_series_la_SOURCES = gand_get_series.c
gand_get_series_la_SOURCES += gand_handle.h
gand_get_series_la_SOURCES += intern.c intern.h
gand_get_series_la_LDFLAGS = $(AM_LDFLAGS)
gand_get_series_la_LDFLAGS += $(MOD_LDFLAGS)
gand_get_series_la_LDFLAGS += $(XCCLDFLAGS)
gand_get_series_la_CPPFLAGS = $(AM_CPPFLAGS)
gand_get_series_la_CPPFLAGS += $(octlab_CFLAGS)
gand_get_series_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_get_series_la_CPPFLAGS += $(matlab_CFLAGS)
gand_get_series_la_CPPFLAGS += -I$(top_srcdir)/src
gand_get_series_la_CPPFLAGS += -DENUM_INTERNS
gand_get_series_la_LIBADD = $(top_builddir)/cli/libgandapi.la

mex_LTLIBRARIES += gand_open.la
mex_DATA += gand_open.m
gand_open_la_LDFLAGS = $(AM_LDFLAGS)
gand_open_la_LDFLAGS += $(MOD_LDFLAGS)
gand_open_la_LDFLAGS += $(XCCLDFLAGS)
gand_open_la_CPPFLAGS = $(AM_CPPFLAGS)
gand_open_la_CPPFLAGS += $(octlab_CFLAGS)
gand_open_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_open_la_CPPFLAGS += $(matlab_CFLAGS)
gand_open_la_LIBADD = $(top_builddir)/cli/libgandapi.la
gand_open_la_SOURCES = gand_open.c
gand_open_la_SOURCES += gand_handle.h

mex_LTLIBRARIES += gand_close.la
mex_DATA += gand_close.m
gand_close_la_LDFLAGS = $(AM_LDFLAGS)
gand_close_la_LDFLAGS += $(MOD_LDFLAGS)
gand_close_la_LDFLAGS += $(XCCLDFLAGS)
gand_close_la_CPPFLAGS = $(AM_CPPFLAGS)
gand_close_la_CPPFLAGS += $(octlab_CFLAGS)
gand_close_la_CPPFLAGS += -I$(top_srcdir)/cli
gand_close_la_CPPFLAGS += $(matlab_CFLAGS)
gand_close_la_LIBADD = $(top_builddir)/cli/libgandapi.la
gand_close_la_SOURCES = gand_close.c
gand_close_la_SOURCES += gand_handle.h


## rename .mex -> .mexa64 on the fly
install-data-hook:
	-cd $(DESTDIR)$(mexdir) && mv -f tzconv.mex tzconv.$(MEXEXT)

uninstall-hook:
	-$(RM) -- $(DESTDIR)$(mexdir)/tzconv.$(MEXEXT)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

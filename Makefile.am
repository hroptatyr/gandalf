include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
DISTCLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)
BUILT_SOURCES =

SUBDIRS += build-aux
SUBDIRS += cli
SUBDIRS += src
SUBDIRS += www

DISTCLEANFILES += version.mk
EXTRA_DIST += version.mk.in
BUILT_SOURCES += .version

.version:
	$(AM_V_GEN) echo "v$(VERSION)" > $@

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version

## Makefile.am ends here

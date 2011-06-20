### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

aouhdrdir = $(includedir)/aou
aouhdr_HEADERS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
lib_LTLIBRARIES =
BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST =

lib_LTLIBRARIES += libgandalf.la
aouhdr_HEADERS += gandalf.h
noinst_HEADERS += gandalf-private.h
noinst_HEADERS += epoll-helpers.h
noinst_HEADERS += ud-sock.h
noinst_HEADERS += nifty.h
libgandalf_la_SOURCES =
libgandalf_la_SOURCES += gand_msg.c
libgandalf_la_SOURCES += gand_msg-scanner.l
libgandalf_la_SOURCES += gand_msg-parser.y
EXTRA_libgandalf_la_SOURCES =
EXTRA_libgandalf_la_SOURCES += $(BUILT_SOURCES)
BUILT_SOURCES += gand_msg-parser.c gand_msg-parser.h
BUILT_SOURCES += gand_msg-scanner.c

## our rule for gperf
%.c: %.gperf
	gperf $< --output-file $@

## Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
%.i: %.c
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
%.s: %.c
	$(COMPILE) -S -c $(AM_CFLAGS) $<

## Makefile.am ends here

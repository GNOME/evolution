## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libibex.la

libibex_la_SOURCES = file.c index.c find.c words.c ibex.h
libibex_la_LDFLAGS = -static

noinst_HEADERS = ibex_internal.h

INCLUDES = -I$(srcdir) $(GLIB_CFLAGS) $(UNICODE_CFLAGS)


noinst_PROGRAMS = mkindex lookup

mkindex_SOURCES = mkindex.c
mkindex_LDADD = libibex.la $(GLIB_LIBS) $(UNICODE_LIBS)

lookup_SOURCES = lookup.c
lookup_LDADD = libibex.la $(GLIB_LIBS) $(UNICODE_LIBS)


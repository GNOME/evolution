## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libibex.a

libibex_a_SOURCES = file.c index.c find.c words.c ibex.h

noinst_HEADERS = ibex_internal.h

INCLUDES = -I$(srcdir) $(GLIB_CFLAGS) $(UNICODE_CFLAGS)


noinst_PROGRAMS = mkindex lookup

mkindex_SOURCES = mkindex.c

mkindex_LDADD = libibex.a $(GLIB_LIBS) $(UNICODE_LIBS)

lookup_SOURCES = lookup.c

lookup_LDADD = libibex.a $(GLIB_LIBS) $(UNICODE_LIBS)

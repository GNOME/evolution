## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libibex.la

libibex_la_SOURCES = \
	wordindex.c \
	block.c ibex.h \
	hash.c \
	disktail.c \
	ibex_block.c

libibex_la_LDFLAGS = -static

noinst_HEADERS = \
	ibex_internal.h \
	block.h \
	wordindex.h \
	index.h

INCLUDES = -I$(srcdir) $(GLIB_CFLAGS) $(UNICODE_CFLAGS) \
		-DG_LOG_DOMAIN=\"libibex\"


#noinst_PROGRAMS = hash

#hash_SOURCES = disktail.c
#hash_LDADD = libibex.la $(GLIB_LIBS) $(UNICODE_LIBS)

#noinst_PROGRAMS = mkindex lookup
#
#mkindex_SOURCES = mkindex.c
#mkindex_LDADD = libibex.la $(GLIB_LIBS) $(UNICODE_LIBS)
#
#lookup_SOURCES = lookup.c
#lookup_LDADD = libibex.la $(GLIB_LIBS) $(UNICODE_LIBS)


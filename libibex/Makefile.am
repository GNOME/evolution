## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libibex.la

libibex_la_SOURCES = \
	wordindex.c wordindexmem.c \
	block.c ibex.h \
	hash.c \
	disktail.c \
	ibex_block.c

libibex_la_LDFLAGS = -static

libibex_la_LIBADD = \
	$(GAL_LIBS)

noinst_HEADERS = \
	ibex_internal.h \
	block.h \
	wordindex.h \
	index.h

INCLUDES = \
	$(EXTRA_GNOME_CFLAGS) \
	-DG_LOG_DOMAIN=\"libibex\"


noinst_PROGRAMS = dumpindex testindex

dumpindex_SOURCES = dumpindex.c
dumpindex_LDADD = libibex.la $(GAL_LIBS) $(THREADS_LIBS)

testindex_SOURCES = testindex.c
testindex_LDADD = libibex.la $(GAL_LIBS) $(THREADS_LIBS) -lm

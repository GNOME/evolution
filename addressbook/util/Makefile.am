INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/camel				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES = libeabutil.la

libeabutil_la_SOURCES =					\
	eab-marshal.c					\
	e-destination.c					\
	e-destination.h					\
	eab-book-util.c					\
	eab-book-util.h

libeabutil_la_LIBADD =					\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/e-util/libeutil.la

MARSHAL_GENERATED = eab-marshal.c eab-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES    = $(BUILT_SOURCES)

EXTRA_DIST = 		\
	eab-marshal.list

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

privsolib_LTLIBRARIES = libeabutil.la

libeabutil_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-DPREFIX=\"$(prefix)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

libeabutil_la_SOURCES =					\
	eab-book-util.c					\
	eab-book-util.h

libeabutil_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libeabutil_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

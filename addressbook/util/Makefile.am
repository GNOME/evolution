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
	$(CAMEL_CFLAGS)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GTKHTML_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

libeabutil_la_SOURCES =					\
	eab-book-util.c					\
	eab-book-util.h

libeabutil_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libeabutil_la_LIBADD =					\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/shell/libevolution-shell.la	\
	$(CAMEL_LIBS)					\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GTKHTML_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

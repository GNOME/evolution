webextensions_LTLIBRARIES = libmodulemailwebextension.la

libmodulemailwebextension_la_SOURCES =			\
	module-mail-web-extension.c			\
	module-mail-web-extension.h

libmodulemailwebextension_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libmodulemailwebextension_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/web-extensions/libedomutils.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmodulemailwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

-include $(top_srcdir)/git.mk

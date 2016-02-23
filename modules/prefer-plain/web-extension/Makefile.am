webextensions_LTLIBRARIES = libmodulepreferplainwebextension.la

libmodulepreferplainwebextension_la_SOURCES =		\
	module-prefer-plain-web-extension.c		\
	module-prefer-plain-web-extension.h

libmodulepreferplainwebextension_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libmodulepreferplainwebextension_la_LIBADD =		\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/web-extensions/libedomutils.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmodulepreferplainwebextension_la_LDFLAGS =		\
	-module -avoid-version -no-undefined

-include $(top_srcdir)/git.mk

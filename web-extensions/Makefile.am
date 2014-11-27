webextensions_LTLIBRARIES = libevolutionwebextension.la

libevolutionwebextension_la_SOURCES =			\
	e-web-extension.c				\
	e-web-extension.h				\
	e-web-extension-main.c

libevolutionwebextension_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libevolutionwebextension_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libevolutionwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

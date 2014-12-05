webextensions_LTLIBRARIES = libewebextension.la

libewebextension_la_SOURCES =				\
	e-web-extension.c				\
	e-web-extension.h				\
	e-web-extension-names.h				\
	e-web-extension-main.c

libewebextension_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libewebextension_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libewebextension_la_LDFLAGS =				\
	-module -avoid-version -no-undefined

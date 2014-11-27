webextensions_LTLIBRARIES = libehtmleditorwebextension.la

libevolutionwebextension_la_SOURCES =			\
	e-html-editor-web-extension.c			\
	e-html-editor-web-extension.h			\
	e-html-editor-web-extension-main.c

libevolutionwebextension_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)				\
	$(WEB_EXTENSIONS_CFLAGS)

libevolutionwebextension_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(GTKHTML_LIBS)					\
	$(WEB_EXTENSIONS_LIBS)

libevolutionwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

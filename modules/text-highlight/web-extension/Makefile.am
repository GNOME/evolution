webextensions_LTLIBRARIES = libmoduletexthighlightwebextension.la

libmoduletexthighlightwebextension_la_SOURCES =		\
	module-text-highlight-web-extension.c		\
	module-text-highlight-web-extension.h

libmoduletexthighlightwebextension_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)				\
	$(WEB_EXTENSIONS_CFLAGS)

libmoduletexthighlightwebextension_la_LIBADD =		\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(GTKHTML_LIBS)					\
	$(WEB_EXTENSIONS_LIBS)

libmoduletexthighlightwebextension_la_LDFLAGS =		\
	-module -avoid-version -no-undefined

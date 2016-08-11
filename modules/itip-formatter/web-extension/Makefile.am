webextensions_LTLIBRARIES = libmoduleitipformatterwebextension.la

libmoduleitipformatterwebextension_la_SOURCES =		\
	module-itip-formatter-web-extension.c		\
	module-itip-formatter-web-extension.h		\
	module-itip-formatter-dom-utils.c		\
	module-itip-formatter-dom-utils.h

libmoduleitipformatterwebextension_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libmoduleitipformatterwebextension_la_LIBADD =		\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/web-extensions/libedomutils.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmoduleitipformatterwebextension_la_LDFLAGS =		\
	-module -avoid-version -no-undefined

-include $(top_srcdir)/git.mk

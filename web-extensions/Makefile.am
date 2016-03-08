SUBDIRS = . composer

webextensions_LTLIBRARIES = libewebextension.la libedomutils.la

libedomutils_la_SOURCES =				\
	e-dom-utils.h					\
	e-dom-utils.c

libedomutils_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

libedomutils_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)

libewebextension_la_SOURCES =				\
	e-web-extension.h				\
	e-web-extension-names.h				\
	e-dom-utils.h					\
	e-web-extension.c				\
	e-web-extension-main.c				\
	e-dom-utils.c

libewebextension_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
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

-include $(top_srcdir)/git.mk

webextensions_LTLIBRARIES = libevolutionwebextension.la

libevolutionwebextension_la_SOURCES =			\
	evolution-web-extension.c			\
	evolution-web-extension.h			\
	$(top_srcdir)/e-util/e-dom-utils.c		\
	$(top_srcdir)/e-util/e-dom-utils.h

libevolutionwebextension_la_CPPFLAGS =			\
	-DWEBEXTENSIONS_COMPILATION			\
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"		\
	-I$(top_srcdir)/e-util				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(AM_CPPFLAGS)					\
	$(WEB_EXTENSIONS_CFLAGS)

libevolutionwebextension_la_LIBADD =			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libevolutionwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

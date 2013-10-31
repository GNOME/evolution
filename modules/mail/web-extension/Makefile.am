webextensions_LTLIBRARIES = libmodulemailwebextension.la

libmodulemailwebextension_la_SOURCES =			\
	module-mail-web-extension.c			\
	module-mail-web-extension.h			\
	$(top_srcdir)/e-util/e-dom-utils.c		\
	$(top_srcdir)/e-util/e-dom-utils.h

libmodulemailwebextension_la_CPPFLAGS =			\
	-DWEBEXTENSIONS_COMPILATION			\
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"		\
	-I$(top_srcdir)/e-util				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(AM_CPPFLAGS)					\
	$(WEB_EXTENSIONS_CFLAGS)

libmodulemailwebextension_la_LIBADD =			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmodulemailwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

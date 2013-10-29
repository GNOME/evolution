webextensions_LTLIBRARIES = libmoduleitipformatterwebextension.la

libmoduleitipformatterwebextension_la_SOURCES =		\
	module-itip-formatter-web-extension.c		\
	module-itip-formatter-web-extension.h		\
	../module-itip-formatter-dom-utils.c		\
	../module-itip-formatter-dom-utils.h		\
	$(top_srcdir)/e-util/e-dom-utils.c		\
	$(top_srcdir)/e-util/e-dom-utils.h

libmoduleitipformatterwebextension_la_CPPFLAGS =	\
	-DWEBEXTENSIONS_COMPILATION			\
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"		\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(AM_CPPFLAGS)					\
	$(WEB_EXTENSIONS_CFLAGS)

libmoduleitipformatterwebextension_la_LIBADD =		\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmoduleitipformatterwebextension_la_LDFLAGS =		\
	-module -avoid-version -no-undefined

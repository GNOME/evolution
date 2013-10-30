webextensions_LTLIBRARIES = libmoduletexthighlightwebextension.la

libmoduletexthighlightwebextension_la_SOURCES =		\
	module-text-highlight-web-extension.c		\
	module-text-highlight-web-extension.h

libmoduletexthighlightwebextension_la_CPPFLAGS =	\
	-DWEBEXTENSIONS_COMPILATION			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(AM_CPPFLAGS)					\
	$(WEB_EXTENSIONS_CFLAGS)

libmoduletexthighlightwebextension_la_LIBADD =		\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmoduletexthighlightwebextension_la_LDFLAGS =		\
	-module -avoid-version -no-undefined

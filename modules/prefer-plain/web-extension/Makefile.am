webextensions_LTLIBRARIES = libmodulepreferplainwebextension.la

libmodulepreferplainwebextension_la_SOURCES =		\
	module-prefer-plain-web-extension.c		\
	module-prefer-plain-web-extension.h

libmodulepreferplainwebextension_la_CPPFLAGS =		\
	-DWEBEXTENSIONS_COMPILATION			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(AM_CPPFLAGS)					\
	$(WEB_EXTENSIONS_CFLAGS)

libmodulepreferplainwebextension_la_LIBADD =		\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmodulepreferplainwebextension_la_LDFLAGS =		\
	-module -avoid-version -no-undefined

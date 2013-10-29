webextensions_LTLIBRARIES = libmodulemailwebextension.la

libmodulemailwebextension_la_SOURCES =			\
	module-mail-web-extension.c			\
	module-mail-web-extension.h

libmodulemailwebextension_la_CPPFLAGS =			\
	-DWEBEXTENSIONS_COMPILATION			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(AM_CPPFLAGS)					\
	$(WEB_EXTENSIONS_CFLAGS)

libmodulemailwebextension_la_LIBADD =			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libmodulemailwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

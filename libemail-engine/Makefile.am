
lib_LTLIBRARIES = libemail-engine.la

AM_CFLAGS = -I$(top_builddir) @GNOME_PLATFORM_CFLAGS@ -Wall -Werror -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = @GNOME_PLATFORM_LIBS@ 


libemail_engine_la_CPPFLAGS = \
	$(AM_CPPFLAGS)	\
	$(EMAIL_ENGINE_CFLAGS)	\
	$(CAMEL_CFLAGS)		\
	$(GNOME_PLATFORM_CFLAGS)

libemail_engine_la_LIBADD = 	\
		$(CAMEL_LIBS)		\
		$(EMAIL_ENGINE_LIBS)	\
		$(GNOME_PLATFORM_LIBS)	\
		$(top_builddir)/libemail-utils/libemail-utils.la
		

libemail_engine_la_LDFLAGS = 

libemail_engine_la_SOURCES = 	\
		e-mail-folder-utils.c	\
		e-mail-store-utils.c	\
		mail-tools.c		\
		e-mail-utils.c		\
		mail-config.c		\
		mail-folder-cache.c	\
		e-mail-session-utils.c	\
		mail-ops.c		\
		e-mail-junk-filter.c	\
		e-mail-session.c	

libmailengineincludedir = $(privincludedir)/libemail-engine
libmailengineinclude_HEADERS = 	\
		e-mail-session.h	\
		e-mail-enums.h		\
		mail-folder-cache.h	\
		e-mail-local.h		\
		e-mail-folder-utils.h	\
		e-mail-store-utils.h	\
		mail-tools.h		\
		e-mail-utils.h		\
		mail-config.h		\
		e-mail-session-utils.h	\
		mail-ops.h		\
		e-mail-junk-filter.h	\
		e-mail-store.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-engine.pc

BUILT_SOURCES = 

EXTRA_DIST = 
CLEANFILES    = $(BUILT_SOURCES)
DISTCLEANFILES = $(pkgconfig_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

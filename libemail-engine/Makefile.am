
lib_LTLIBRARIES = libemail-engine.la

AM_CFLAGS = -I$(top_builddir) @GNOME_PLATFORM_CFLAGS@ -Wall -Werror -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = @GNOME_PLATFORM_LIBS@ 

include $(top_srcdir)/glib-gen.mak
glib_enum_headers=e-mail-enums.h
glib_enum_output=e-mail-enumtypes
glib_enum_define=E_MAIL
glib_enum_prefix=e_mail

ENUM_GENERATED = e-mail-enumtypes.h e-mail-enumtypes.c

libemail_engine_la_CPPFLAGS = \
	$(AM_CPPFLAGS)	\
	$(CAMEL_CFLAGS)		\
	$(GNOME_PLATFORM_CFLAGS)

libemail_engine_la_LIBADD = 	\
		$(CAMEL_LIBS)		\
		$(GNOME_PLATFORM_LIBS)	\
		$(top_builddir)/libemail-utils/libemail-utils.la
		

libemail_engine_la_LDFLAGS = 

libemail_engine_la_SOURCES = 	\
		e-mail-enumtypes.c	\
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
		e-mail-enums.h		\
		e-mail-enumtypes.h	\
		e-mail-session.h	\
		e-mail-enums.h		\
		e-mail-session.c	\
		e-mail-folder-utils.h	\
		e-mail-store-utils.h	\
		mail-tools.h		\
		e-mail-utils.h		\
		mail-config.h		\
		e-mail-session-utils.h	\
		mail-ops.h		\
		e-mail-junk-filter.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-engine.pc

BUILT_SOURCES = $(ENUM_GENERATED)

EXTRA_DIST = 
CLEANFILES    = $(BUILT_SOURCES)
DISTCLEANFILES = $(pkgconfig_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

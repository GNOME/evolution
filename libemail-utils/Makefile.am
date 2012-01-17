
lib_LTLIBRARIES = libemail-utils.la

AM_CFLAGS = -I. -I$(top_builddir) @GNOME_PLATFORM_CFLAGS@ -Wall -Werror -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = @GNOME_PLATFORM_LIBS@ 

include $(top_srcdir)/glib-gen.mak
glib_enum_prefix=e
MARSHAL_GENERATED = e-marshal.c e-marshal.h

libemail_utils_la_CPPFLAGS = \
	$(AM_CPPFLAGS)	\
	$(CAMEL_CFLAGS)		\
	$(GNOME_PLATFORM_CFLAGS) \
	$(EMAIL_UTILS_CFLAGS)	\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	

libemail_utils_la_LIBADD = 	\
		$(CAMEL_LIBS)		\
		$(GNOME_PLATFORM_LIBS)	\
		$(EMAIL_UTILS_LIBS)

libemail_utils_la_LDFLAGS = 

libemail_utils_la_SOURCES = 	\
		e-marshal.c	\
		e-account-utils.c	\
		mail-mt.c	\
		e-signature.c	\
		e-signature-list.c	\
		e-signature-utils.c


libmailutilsincludedir = $(privincludedir)/libemail-utils
libmailutilsinclude_HEADERS = 	\
		e-marshal.h	\
		e-account-utils.h	\
		mail-mt.h	\
		e-signature.h	\
		e-signature-list.h	\
		e-signature-utils.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-utils.pc

BUILT_SOURCES = $(MARSHAL_GENERATED) 

EXTRA_DIST = e-marshal.list
CLEANFILES    = $(BUILT_SOURCES)
DISTCLEANFILES = $(pkgconfig_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)


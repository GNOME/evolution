
lib_LTLIBRARIES = libemail-utils.la

AM_CFLAGS = -I. -I$(top_builddir) @GNOME_PLATFORM_CFLAGS@ -Wall -Werror -g3 -O0 -ggdb -DPKGDATADIR="\"$(pkgdatadir)\""
AM_LDFLAGS = @GNOME_PLATFORM_LIBS@ 

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
		e-account-utils.c	\
		mail-mt.c	\
		e-signature.c	\
		e-signature-list.c	\
		e-signature-utils.c


libmailutilsincludedir = $(privincludedir)/libemail-utils
libmailutilsinclude_HEADERS = 	\
		e-account-utils.h	\
		mail-mt.h	\
		e-signature.h	\
		e-signature-list.h	\
		e-signature-utils.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-utils.pc

DISTCLEANFILES = $(pkgconfig_DATA)

-include $(top_srcdir)/git.mk

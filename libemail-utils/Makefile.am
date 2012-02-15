NULL =

privsolib_LTLIBRARIES = libemail-utils.la

libemail_utils_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(GNOME_PLATFORM_CFLAGS) \
	$(NULL)

libmailutilsincludedir = $(privincludedir)/libemail-utils
libmailutilsinclude_HEADERS = \
	e-account-utils.h \
	e-signature-list.h \
	e-signature-utils.h \
	e-signature.h \
	mail-mt.h \
	$(NULL)

libemail_utils_la_SOURCES = \
	$(libmailutilsinclude_HEADERS) \
	e-account-utils.c \
	e-signature-list.c \
	e-signature-utils.c \
	e-signature.c \
	mail-mt.c \
	$(NULL)

libemail_utils_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libemail_utils_la_LIBADD = \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(GNOME_PLATFORM_LIBS) \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libemail-utils.pc

DISTCLEANFILES = $(pkgconfig_DATA)

-include $(top_srcdir)/git.mk

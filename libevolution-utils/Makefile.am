NULL =

privsolib_LTLIBRARIES = libevolution-utils.la

libevolution_utils_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(GNOME_PLATFORM_CFLAGS) \
	$(NULL)

libevolutionutilsincludedir = $(privincludedir)/libevolution-utils
libevolutionutilsinclude_HEADERS = \
	$(NULL)

libevolution_utils_la_SOURCES = \
	$(libevolutionutilsinclude_HEADERS) \
	$(NULL)

libevolution_utils_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libevolution_utils_la_LIBADD = \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(GNOME_PLATFORM_LIBS) \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libevolution-utils.pc

DISTCLEANFILES = $(pkgconfig_DATA)

-include $(top_srcdir)/git.mk

privsolib_LTLIBRARIES = libevolution-addressbook-importers.la

libevolution_addressbook_importers_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)					\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_builddir)/addressbook			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

libevolution_addressbook_importers_la_SOURCES = \
	evolution-ldif-importer.c		\
	evolution-vcard-importer.c		\
	evolution-csv-importer.c	        \
	evolution-addressbook-importers.h

libevolution_addressbook_importers_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolution_addressbook_importers_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la 			\
	$(top_builddir)/shell/libevolution-shell.la			\
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(IMPORTERS_LIBS)						\
	$(NULL)

-include $(top_srcdir)/git.mk

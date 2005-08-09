
privsolib_LTLIBRARIES = 				\
	libevolution-addressbook-importers.la

INCLUDES = 						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_builddir)/addressbook			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

libevolution_addressbook_importers_la_SOURCES = \
	evolution-ldif-importer.c		\
	evolution-vcard-importer.c		\
	evolution-addressbook-importers.h

libevolution_addressbook_importers_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_addressbook_importers_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la 				\
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(IMPORTERS_LIBS)

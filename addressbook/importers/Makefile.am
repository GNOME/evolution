privsolib_LTLIBRARIES = libevolution-addressbook-importers.la

libevolution_addressbook_importers_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)					\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)/addressbook			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

libevolution_addressbook_importers_la_SOURCES = \
	evolution-ldif-importer.c		\
	evolution-vcard-importer.c		\
	evolution-csv-importer.c	        \
	evolution-addressbook-importers.h

libevolution_addressbook_importers_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_addressbook_importers_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la 				\
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(IMPORTERS_LIBS)

-include $(top_srcdir)/git.mk

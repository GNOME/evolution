importersdir = $(privlibdir)/evolution-addressbook-importers

importers_LTLIBRARIES = 				\
	libevolution-addressbook-ldif-importer.la	\
	libevolution-addressbook-vcard-importer.la

INCLUDES = 						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\"		\
	-DG_LOG_DOMAIN=\"Evolution-Importer\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/addressbook			\
	-I$(top_builddir)/addressbook			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

# VCard Importer
libevolution_addressbook_vcard_importer_la_SOURCES =	\
	evolution-vcard-importer.c

libevolution_addressbook_vcard_importer_la_LDFLAGS = -avoid-version -module

libevolution_addressbook_vcard_importer_la_LIBADD = \
	$(top_builddir)/shell/importer/libevolution-importer.la 	\
	$(top_builddir)/e-util/libeutil.la 				\
	$(IMPORTERS_LIBS)

# LDIF Importer
libevolution_addressbook_ldif_importer_la_SOURCES =	\
	evolution-ldif-importer.c

libevolution_addressbook_ldif_importer_la_LDFLAGS = -avoid-version -module

libevolution_addressbook_ldif_importer_la_LIBADD = \
	$(top_builddir)/shell/importer/libevolution-importer.la 	\
	$(top_builddir)/e-util/libeutil.la 				\
	$(IMPORTERS_LIBS)

server_in_files = 						\
	GNOME_Evolution_Addressbook_LDIF_Importer.server.in.in	\
	GNOME_Evolution_Addressbook_VCard_Importer.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(server_in_files)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

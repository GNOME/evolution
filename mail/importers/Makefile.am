
privlib_LTLIBRARIES = libevolution-mail-importers.la

INCLUDES = -I.. 					\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"	\
	-I$(top_srcdir)/addressbook/backend             \
	-I$(top_builddir)/addressbook/backend           \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(IMPORTERS_CFLAGS)

libevolution_mail_importers_la_SOURCES =	\
	mail-importer.c				\
	mail-importer.h				\
	elm-importer.c				\
	pine-importer.c				\
	netscape-importer.c			\
	evolution-outlook-importer.c		\
	evolution-mbox-importer.c

libevolution_mail_importers_la_LIBADD =				\
	$(top_builddir)/shell/importer/libevolution-importer.la	\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(IMPORTERS_LIBS)

server_in_files = \
	GNOME_Evolution_Mail_Importers.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(server_in_files)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

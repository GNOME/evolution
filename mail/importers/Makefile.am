importersdir = $(privlibdir)/evolution-mail-importers

importers_LTLIBRARIES = liboutlook.la libmbox.la

INCLUDES = -I.. 					\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-I$(includedir)					\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"	\
	$(IMPORTERS_CFLAGS)

liboutlook_la_SOURCES =				\
	evolution-outlook-importer.c
liboutlook_la_LDFLAGS = -avoid-version -module

libmbox_la_SOURCES = evolution-mbox-importer.c \
		     mozilla-status-headers.h
libmbox_la_LDFLAGS = -avoid-version -module

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Evolution_Mail_Mbox_Importer.server.in.in	\
	GNOME_Evolution_Mail_Outlook_Importer.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(server_in_files) $(server_DATA)

@INTLTOOL_SERVER_RULE@

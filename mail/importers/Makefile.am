importersdir = $(pkglibdir)/evolution-mail-importers/$(VERSION)

importers_LTLIBRARIES = liboutlook.la libmbox.la libiinetscape.la

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/shell			\
	-I$(top_builddir)/shell			\
	-I$(includedir)				\
	$(GNOME_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"

liboutlook_la_SOURCES =				\
	evolution-outlook-importer.c
liboutlook_la_LDFLAGS = -version-info 0:0:0

libmbox_la_SOURCES = evolution-mbox-importer.c
libmbox_la_LDFLAGS = -version-info 0:0:0

libiinetscape_la_SOURCES = netscape-importer.c
libiinetscape_la_LDFLAGS = -version-info 0:0:0

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Mail_Mbox_Importer.oaf.in	\
	GNOME_Evolution_Mail_Outlook_Importer.oaf.in		\
	GNOME_Evolution_Mail_Netscape_Intelligent_Importer.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

importersdir = $(pkglibdir)/evolution-mail-importers/$(VERSION)

importers_LTLIBRARIES = liboutlook.la libmbox.la

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/shell			\
	-I$(top_builddir)/shell			\
	-I$(includedir)				\
	$(EXTRA_GNOME_CFLAGS)			\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"

liboutlook_la_SOURCES =				\
	evolution-outlook-importer.c
liboutlook_la_LDFLAGS = -version-info 0:0:0

libmbox_la_SOURCES = evolution-mbox-importer.c
libmbox_la_LDFLAGS = -version-info 0:0:0

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Mail_Mbox_Importer.oaf.in	\
	GNOME_Evolution_Mail_Outlook_Importer.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)

@XML_I18N_MERGE_OAF_RULE@

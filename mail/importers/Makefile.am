importersdir = $(pkglibdir)/evolution-mail-importers/$(VERSION)

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

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Mail_Mbox_Importer.oaf.in	\
	GNOME_Evolution_Mail_Outlook_Importer.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)

@XML_I18N_MERGE_OAF_RULE@

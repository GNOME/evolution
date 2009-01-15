INCLUDES =						\
	-I$(EVOLUTION_SOURCE)				\
	-I$(top_srcdir)					\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	-DLOCALEDIR="\"$(LOCALEDIR)\""			\
	$(EVOLUTION_CFLAGS)				\
	$(EVOLUTION_MAIL_CFLAGS)			

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-pst-import.eplug
plugin_LTLIBRARIES = liborg-gnome-pst-import.la

liborg_gnome_pst_import_la_SOURCES = pst-importer.c
liborg_gnome_pst_import_la_LDFLAGS = -module -avoid-version
liborg_gnome_pst_import_la_LIBADD = \
	$(LIBPST_LIBS)

EXTRA_DIST = org-gnome-pst-import.eplug.xml

BUILT_SOURCES = org-gnome-pst-import.eplug

CLEANFILES = $(BUILT_SOURCES)


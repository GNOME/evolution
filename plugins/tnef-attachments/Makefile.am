INCLUDES =						\
	-I$(top_srcdir)					\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	-DLOCALEDIR="\"$(LOCALEDIR)\""			\	
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-tnef-attachments.eplug
plugin_LTLIBRARIES = liborg-gnome-tnef-attachments.la

liborg_gnome_tnef_attachments_la_SOURCES = tnef-plugin.c
liborg_gnome_tnef_attachments_la_LDFLAGS = -module -avoid-version -lytnef

EXTRA_DIST = org-gnome-tnef-attachments.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-save-attachments.eplug org-gnome-save-attachments.xml
plugin_LTLIBRARIES = liborg-gnome-save-attachments.la

liborg_gnome_save_attachments_la_SOURCES = save-attachments.c
liborg_gnome_save_attachments_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = 					\
	org-gnome-save-attachments.eplug.in	\
	org-gnome-save-attachments.xml
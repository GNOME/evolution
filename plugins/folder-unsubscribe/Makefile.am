INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-folder-unsubscribe.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-folder-unsubscribe.la

liborg_gnome_mail_folder_unsubscribe_la_SOURCES = folder-unsubscribe.c
liborg_gnome_mail_folder_unsubscribe_la_LDFLAGS = -module -avoid-version

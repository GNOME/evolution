INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-to-task.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-to-task.la

liborg_gnome_mail_to_task_la_SOURCES = mail-to-task.c
liborg_gnome_mail_to_task_la_LDFLAGS = -module -avoid-version

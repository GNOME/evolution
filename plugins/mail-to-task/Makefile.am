NCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-to-task.eplug org-gnome-mail-to-task.xml
plugin_LTLIBRARIES = liborg-gnome-mail-to-task.la

liborg_gnome_mail_to_task_la_SOURCES = mail-to-task.c
liborg_gnome_mail_to_task_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-mail-to-task.eplug.xml \
	     org-gnome-mail-to-task.xml

BUILT_SOURCES = org-gnome-mail-to-task.eplug
CLEANFILES = $(BUILT_SOURCES)

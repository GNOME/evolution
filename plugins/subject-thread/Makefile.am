INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-subject-thread.eplug
plugin_LTLIBRARIES = liborg-gnome-subject-thread.la

liborg_gnome_subject_thread_la_SOURCES = subject-thread.c
liborg_gnome_subject_thread_la_LDFLAGS = -module -avoid-version

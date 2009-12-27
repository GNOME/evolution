plugin_LTLIBRARIES = liborg-gnome-subject-thread.la

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-subject-thread.eplug

liborg_gnome_subject_thread_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

liborg_gnome_subject_thread_la_SOURCES = subject-thread.c
liborg_gnome_subject_thread_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_subject_thread_la_LIBADD =			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = org-gnome-subject-thread.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

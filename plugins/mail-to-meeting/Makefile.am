INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-to-meeting.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-to-meeting.la

liborg_gnome_mail_to_meeting_la_SOURCES = mail-to-meeting.c
liborg_gnome_mail_to_meeting_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_mail_to_meeting_la_LIBADD =	\
	$(EVOLUTION_CALENDAR_LIBS)	\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = org-gnome-mail-to-meeting.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

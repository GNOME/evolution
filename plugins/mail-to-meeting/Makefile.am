INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-to-meeting.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-to-meeting.la

liborg_gnome_mail_to_meeting_la_SOURCES = mail-to-meeting.c
liborg_gnome_mail_to_meeting_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-mail-to-meeting.eplug.in
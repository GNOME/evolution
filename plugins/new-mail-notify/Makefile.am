INCLUDES =						\
	-I$(top_srcdir)					\
	-DDBUS_API_SUBJECT_TO_CHANGE=1		\
	$(EVOLUTION_MAIL_CFLAGS)				\
	$(NMN_CFLAGS)						

LIBS =							\
	$(NMN_LIBS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-new-mail-notify.eplug
plugin_LTLIBRARIES = liborg-gnome-new-mail-notify.la

liborg_gnome_new_mail_notify_la_SOURCES = new-mail-notify.c
liborg_gnome_new_mail_notify_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-new-mail-notify.eplug.in

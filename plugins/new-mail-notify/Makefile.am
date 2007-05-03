INCLUDES =						\
	-I$(top_srcdir)					\
	-DDBUS_API_SUBJECT_TO_CHANGE=1		\
	-DDBUS_VERSION=$(DBUS_VERSION)          \
	$(EVOLUTION_MAIL_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)
	$(NMN_CFLAGS)						

LIBS =							\
	$(LIBNOTIFY_LIBS)				\
	$(NMN_LIBS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-notification.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-notification.la

liborg_gnome_mail_notification_la_SOURCES = mail-notification.c
liborg_gnome_mail_notification_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-mail-notification.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(GNOME_PLATFORM_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-notification.eplug

plugin_LTLIBRARIES = liborg-gnome-mail-notification.la

liborg_gnome_mail_notification_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)		\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)			\
	$(CANBERRA_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_mail_notification_la_SOURCES = mail-notification.c

liborg_gnome_mail_notification_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_mail_notification_la_LIBADD = 			\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LIBNOTIFY_LIBS)					\
	$(CANBERRA_LIBS)					\
	$(GTKHTML_LIBS)

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = 					\
	org-gnome-mail-notification.eplug.xml

-include $(top_srcdir)/git.mk

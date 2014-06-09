@EVO_PLUGIN_RULE@

plugin_DATA = 					\
	org-gnome-publish-calendar.eplug

ui_DATA =							\
	publish-calendar.ui				

plugin_LTLIBRARIES = liborg-gnome-publish-calendar.la

liborg_gnome_publish_calendar_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(LIBNOTIFY_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_publish_calendar_la_SOURCES =	\
	publish-calendar.c			\
	publish-format-fb.c			\
	publish-format-fb.h			\
	publish-format-ical.c			\
	publish-format-ical.h			\
	publish-location.c			\
	publish-location.h			\
	url-editor-dialog.c			\
	url-editor-dialog.h

liborg_gnome_publish_calendar_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

liborg_gnome_publish_calendar_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LIBNOTIFY_LIBS)					\
	$(NULL)

EXTRA_DIST =					\
	org-gnome-publish-calendar.eplug.xml	\
	$(ui_DATA)

CLEANFILES = org-gnome-publish-calendar.eplug

-include $(top_srcdir)/git.mk

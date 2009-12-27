@EVO_PLUGIN_RULE@

plugin_DATA = 					\
	org-gnome-publish-calendar.eplug

ui_DATA =							\
	publish-calendar.ui				

plugin_LTLIBRARIES = liborg-gnome-publish-calendar.la

liborg_gnome_publish_calendar_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)/shell				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(E_UTIL_CFLAGS)				\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"

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

liborg_gnome_publish_calendar_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_publish_calendar_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(E_UTIL_LIBS)						\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST =					\
	org-gnome-publish-calendar.eplug.xml	\
	$(ui_DATA)

CLEANFILES = org-gnome-publish-calendar.eplug

-include $(top_srcdir)/git.mk

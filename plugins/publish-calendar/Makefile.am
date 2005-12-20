INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	-DPLUGINDIR="\"$(plugindir)\""

@EVO_PLUGIN_RULE@

plugin_DATA = 					\
	org-gnome-publish-calendar.eplug	\
	org-gnome-publish-calendar.xml		\
	publish-calendar.glade
plugin_LTLIBRARIES = liborg-gnome-publish-calendar.la

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

liborg_gnome_publish_calendar_la_LDFLAGS = -module -avoid-version

EXTRA_DIST =					\
	org-gnome-publish-calendar.eplug.in	\
	publish-calendar.glade			\
	org-gnome-publish-calendar.xml

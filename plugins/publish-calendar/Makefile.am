AM_CPPFLAGS =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	-I$(top_builddir)/shell			\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\"" \
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"          

@EVO_PLUGIN_RULE@

plugin_DATA = 					\
	org-gnome-publish-calendar.eplug	\
	org-gnome-publish-calendar.xml	

glade_DATA =							\
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

liborg_gnome_publish_calendar_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_publish_calendar_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST =					\
	org-gnome-publish-calendar.eplug.xml	\
	org-gnome-publish-calendar.xml		\
	$(glade_DATA)

CLEANFILES = org-gnome-publish-calendar.eplug

-include $(top_srcdir)/git.mk

INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mark-calendar-offline.eplug
plugin_LTLIBRARIES = liborg-gnome-mark-calendar-offline.la

liborg_gnome_mark_calendar_offline_la_SOURCES = mark-calendar-offline.c
liborg_gnome_mark_calendar_offline_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-mark-calendar-offline.eplug.in
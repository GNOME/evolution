INCLUDES =					\
	$(EVOLUTION_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	$(SOURCE_SEL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-calendar-http.eplug
plugin_LTLIBRARIES = liborg-gnome-calendar-http.la


liborg_gnome_calendar_http_la_SOURCES = calendar-http.c
liborg_gnome_calendar_http_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-calendar-http.eplug.in

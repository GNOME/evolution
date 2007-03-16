INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	$(SOURCE_SEL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-calendar-http.eplug
plugin_LTLIBRARIES = liborg-gnome-calendar-http.la


liborg_gnome_calendar_http_la_SOURCES = calendar-http.c
liborg_gnome_calendar_http_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_calendar_http_la_LIBADD =		\
	$(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST = org-gnome-calendar-http.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

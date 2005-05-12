INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-save-calendar.eplug
plugin_LTLIBRARIES = liborg-gnome-save-calendar.la

liborg_gnome_save_calendar_la_SOURCES = \
	save-calendar.c			\
	ical-format.c			\
	csv-format.c			\
	rdf-format.c			\
	format-handler.h

liborg_gnome_save_calendar_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-save-calendar.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

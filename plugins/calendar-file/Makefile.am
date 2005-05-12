INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	$(SOURCE_SEL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-calendar-file.eplug
plugin_LTLIBRARIES = liborg-gnome-calendar-file.la


liborg_gnome_calendar_file_la_SOURCES = calendar-file.c
liborg_gnome_calendar_file_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-calendar-file.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

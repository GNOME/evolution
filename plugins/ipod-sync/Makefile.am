INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	-DDBUS_API_SUBJECT_TO_CHANGE

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-ipod-sync-evolution.eplug
plugin_LTLIBRARIES = liborg-gnome-ipod-sync-evolution.la

liborg_gnome_ipod_sync_evolution_la_SOURCES = \
	ipod-sync.c			\
	ical-format.c			\
	evolution-ipod-sync.c		\
	ipod.c				\
	format-handler.h

liborg_gnome_ipod_sync_evolution_la_LDFLAGS = -module -avoid-version
liborg_gnome_ipod_sync_evolution_la_LIBADD = 			\
	$(EVOLUTION_CALENDAR_LIBS)


EXTRA_DIST = org-gnome-ipod-sync-evolution.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
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

liborg_gnome_save_calendar_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_save_calendar_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la	\
	$(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST = org-gnome-save-calendar.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

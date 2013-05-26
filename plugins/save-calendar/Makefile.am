@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-save-calendar.eplug

plugin_LTLIBRARIES = liborg-gnome-save-calendar.la

liborg_gnome_save_calendar_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_save_calendar_la_SOURCES = \
	save-calendar.c			\
	ical-format.c			\
	csv-format.c			\
	rdf-format.c			\
	format-handler.h

liborg_gnome_save_calendar_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_save_calendar_la_LIBADD =	\
	$(top_builddir)/e-util/libevolution-util.la \
	$(top_builddir)/shell/libevolution-shell.la \
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)			\
	$(GTKHTML_LIBS)

EXTRA_DIST = org-gnome-save-calendar.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

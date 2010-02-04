@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-calendar-file.eplug

plugin_LTLIBRARIES = liborg-gnome-calendar-file.la

liborg_gnome_calendar_file_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	$(SOURCE_SEL_CFLAGS)

liborg_gnome_calendar_file_la_SOURCES = calendar-file.c

liborg_gnome_calendar_file_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_calendar_file_la_LIBADD = 		\
	$(top_builddir)/e-util/libeutil.la	\
	$(EVOLUTION_CALENDAR_LIBS)		\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = org-gnome-calendar-file.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

eds_datadir = `pkg-config --variable=privdatadir evolution-data-server-1.2`

INCLUDES =									\
	-I$(top_srcdir)								\
	-I$(top_builddir)/shell							\
	$(EVOLUTION_CALENDAR_CFLAGS)						\
	$(SOURCE_SEL_CFLAGS)							\
	-DWEATHER_DATADIR=\""$(weatherdatadir)"\"				\
	-DWEATHER_EDS_DATADIR=\""$(eds_datadir)/weather"\"			\
	$(LIBGWEATHER_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-calendar-weather.eplug
plugin_LTLIBRARIES = liborg-gnome-calendar-weather.la

liborg_gnome_calendar_weather_la_SOURCES = calendar-weather.c
liborg_gnome_calendar_weather_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(LIBGWEATHER_LIBS)
liborg_gnome_calendar_weather_la_LIBADD =			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST =					\
	org-gnome-calendar-weather.eplug.xml	\
	$(weatherdata_DATA)

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

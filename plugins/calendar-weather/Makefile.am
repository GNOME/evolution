INCLUDES =				\
	-I$(top_srcdir)/shell		\
	$(EVOLUTION_CFLAGS)		\
	$(EVOLUTION_CALENDAR_CFLAGS)	\
	$(SOURCE_SEL_CFLAGS)		\
	-DWEATHER_DATADIR=\""$(weatherdatadir)"\"

@EVO_PLUGIN_RULE@

# These will actually install into the e-d-s share directory, but
# if the plugin is disabled, we aren't going to be needing them.
weatherdata_DATA = \
	category_weather_cloudy_16.png		\
	category_weather_fog_16.png		\
	category_weather_partly_cloudy_16.png	\
	category_weather_rain_16.png		\
	category_weather_snow_16.png		\
	category_weather_sun_16.png		\
	category_weather_tstorm_16.png

plugin_DATA = org-gnome-calendar-weather.eplug
plugin_LTLIBRARIES = liborg-gnome-calendar-weather.la

liborg_gnome_calendar_weather_la_SOURCES = calendar-weather.c
liborg_gnome_calendar_weather_la_LDFLAGS = -module -avoid-version

EXTRA_DIST =	\
	org-gnome-calendar-weather.eplug.in	\
	$(weatherdata_DATA)

module_LTLIBRARIES = module-cal-config-weather.la

module_cal_config_weather_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-cal-config-weather\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(GWEATHER_CFLAGS)

module_cal_config_weather_la_SOURCES = \
	evolution-cal-config-weather.c				\
	e-source-weather.c					\
	e-source-weather.h

module_cal_config_weather_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(GWEATHER_LIBS)

module_cal_config_weather_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

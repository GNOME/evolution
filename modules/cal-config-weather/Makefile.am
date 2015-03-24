module_LTLIBRARIES = module-cal-config-weather.la

module_cal_config_weather_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-cal-config-weather\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GWEATHER_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_cal_config_weather_la_SOURCES = \
	evolution-cal-config-weather.c

module_cal_config_weather_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GWEATHER_LIBS)					\
	$(NULL)

module_cal_config_weather_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

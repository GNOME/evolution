module_LTLIBRARIES = module-cal-config-weather.la

module_cal_config_weather_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-cal-config-weather\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GWEATHER_CFLAGS)

module_cal_config_weather_la_SOURCES = \
	evolution-cal-config-weather.c				\
	e-source-weather.c					\
	e-source-weather.h

module_cal_config_weather_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GWEATHER_LIBS)

module_cal_config_weather_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

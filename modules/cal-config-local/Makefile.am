module_LTLIBRARIES = module-cal-config-local.la

module_cal_config_local_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-cal-config-local\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

module_cal_config_local_la_SOURCES = \
	evolution-cal-config-local.c				\
	e-source-local.c					\
	e-source-local.h

module_cal_config_local_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

module_cal_config_local_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

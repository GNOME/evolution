module_LTLIBRARIES = module-cal-config-caldav.la

module_cal_config_caldav_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-cal-config-caldav\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(LIBSOUP_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_cal_config_caldav_la_SOURCES = \
	evolution-cal-config-caldav.c

module_cal_config_caldav_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LIBSOUP_LIBS)						\
	$(NULL)

module_cal_config_caldav_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

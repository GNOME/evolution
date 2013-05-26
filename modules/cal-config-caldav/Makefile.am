module_LTLIBRARIES = module-cal-config-caldav.la

module_cal_config_caldav_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-cal-config-caldav\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(LIBSOUP_CFLAGS)

module_cal_config_caldav_la_SOURCES = \
	evolution-cal-config-caldav.c				\
	e-caldav-chooser.c					\
	e-caldav-chooser.h					\
	e-caldav-chooser-dialog.c				\
	e-caldav-chooser-dialog.h

module_cal_config_caldav_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(LIBSOUP_LIBS)

module_cal_config_caldav_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

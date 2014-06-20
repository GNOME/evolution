module_LTLIBRARIES = module-cal-config-google.la

module_cal_config_google_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-cal-config-google\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(GDATA_CFLAGS)

module_cal_config_google_la_SOURCES = \
	e-cal-config-google.c					\
	e-cal-config-gtasks.c					\
	e-google-chooser-button.c				\
	e-google-chooser-button.h				\
	e-google-chooser-dialog.c				\
	e-google-chooser-dialog.h				\
	e-google-chooser.c					\
	e-google-chooser.h					\
	module-cal-config-google.c				\
	module-cal-config-google.h

module_cal_config_google_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(GDATA_LIBS)

module_cal_config_google_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

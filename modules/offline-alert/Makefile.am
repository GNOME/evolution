module_LTLIBRARIES = module-offline-alert.la

module_offline_alert_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-offline-alert\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_offline_alert_la_SOURCES = \
	evolution-offline-alert.c

module_offline_alert_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

module_offline_alert_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

error_DATA = evolution-offline-alert.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = evolution-offline-alert.error.xml

-include $(top_srcdir)/git.mk

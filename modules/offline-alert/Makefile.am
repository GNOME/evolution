module_LTLIBRARIES = libevolution-module-offline-alert.la

libevolution_module_offline_alert_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-offline-alert\"		\
	$(GNOME_PLATFORM_CFLAGS)

libevolution_module_offline_alert_la_SOURCES =			\
	evolution-offline-alert.c

libevolution_module_offline_alert_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_offline_alert_la_LDFLAGS =			\
	-module -avoid-version $(NO_UNDEFINED)

error_DATA = evolution-offline-alert.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

EXTRA_DIST = evolution-offline-alert.error.xml

-include $(top_srcdir)/git.mk

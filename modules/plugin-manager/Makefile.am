module_LTLIBRARIES = module-plugin-manager.la

module_plugin_manager_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-plugin-manager\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_plugin_manager_la_SOURCES = \
	evolution-plugin-manager.c

module_plugin_manager_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

module_plugin_manager_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

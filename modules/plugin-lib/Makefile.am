module_LTLIBRARIES = module-plugin-lib.la

module_plugin_lib_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-plugin-lib\"			\
	-I$(top_srcdir)						\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""		\
	-DBUILDTIME_EVOLUTION_PLUGINDIR="\"$(plugindir)\""	\
	-DEVOLUTION_PREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_plugin_lib_la_SOURCES = \
	evolution-module-plugin-lib.c				\
	e-plugin-lib.c						\
	e-plugin-lib.h

module_plugin_lib_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

module_plugin_lib_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

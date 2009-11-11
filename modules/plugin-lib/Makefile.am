module_LTLIBRARIES = libevolution-module-plugin-lib.la

libevolution_module_plugin_lib_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-plugin-lib\"			\
	-I$(top_srcdir)						\
	-DEVOLUTION_PREFIX=\""$(prefix)"\"			\
	$(E_UTIL_CFLAGS)

libevolution_module_plugin_lib_la_SOURCES =			\
	evolution-module-plugin-lib.c				\
	e-plugin-lib.c						\
	e-plugin-lib.h

libevolution_module_plugin_lib_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(E_UTIL_LIBS)

libevolution_module_plugin_lib_la_LDFLAGS =			\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

module_LTLIBRARIES = libevolution-module-plugin-mono.la

libevolution_module_plugin_mono_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-plugin-mono\"		\
	-I$(top_srcdir)						\
	$(E_UTIL_CFLAGS)					\
	$(MONO_CFLAGS)

libevolution_module_plugin_mono_la_SOURCES =			\
	evolution-module-plugin-mono.c				\
	e-plugin-mono.c						\
	e-plugin-mono.h

libevolution_module_plugin_mono_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(E_UTIL_LIBS)						\
	$(MONO_LIBS)

libevolution_module_plugin_mono_la_LDFLAGS =			\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

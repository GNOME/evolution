module_LTLIBRARIES = module-plugin-mono.la

module_plugin_mono_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-plugin-mono\"		\
	-I$(top_srcdir)						\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(MONO_CFLAGS)

module_plugin_mono_la_SOURCES = \
	evolution-module-plugin-mono.c				\
	e-plugin-mono.c						\
	e-plugin-mono.h

module_plugin_mono_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(MONO_LIBS)

module_plugin_mono_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

module_LTLIBRARIES = module-plugin-python.la

module_plugin_python_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-plugin-python\"		\
	-I$(top_srcdir)						\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(PY_INCLUDES)

module_plugin_python_la_SOURCES = \
	evolution-module-plugin-python.c			\
	e-plugin-python.c					\
	e-plugin-python.h

module_plugin_python_la_LIBADD = \
	-ldl -lutil -lm						\
	$(top_builddir)/e-util/libeutil.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(PY_LIBS)

module_plugin_python_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

example_sources =						\
	example/hello_python.py					\
	example/org-gnome-hello-python-ui.xml			\
	example/org-gnome-hello-python.eplug.xml		\
	example/Makefile.am

EXTRA_DIST = $(example_sources)

-include $(top_srcdir)/git.mk

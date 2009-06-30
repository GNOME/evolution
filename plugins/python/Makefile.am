AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	$(E_UTIL_CFLAGS)				\
	$(PY_INCLUDES) 

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-python.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-python.la

liborg_gnome_evolution_python_la_SOURCES = python-plugin-loader.c python-plugin-loader.h
liborg_gnome_evolution_python_la_LDFLAGS = -module -avoid-version
liborg_gnome_evolution_python_la_LIBADD =	\
	-lpthread -ldl -lutil -lm \
	$(PY_LIBS) \
	$(E_UTIL_LIBS)

example_sources =					\
	example/hello_python.py 			\
	example/org-gnome-hello-python-ui.xml		\
	example/org-gnome-hello-python.eplug.xml	\
	example/Makefile.am	

EXTRA_DIST = org-gnome-evolution-python.eplug.xml	\
	$(example_sources)

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

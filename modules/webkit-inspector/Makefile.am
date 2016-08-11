module_LTLIBRARIES = module-webkit-inspector.la

module_webkit_inspector_la_CPPFLAGS = 			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"evolution-webkit-inspector\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

module_webkit_inspector_la_SOURCES = \
	evolution-webkit-inspector.c

module_webkit_inspector_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

module_webkit_inspector_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

module_LTLIBRARIES = libevolution-module-mdn.la

libevolution_module_mdn_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-mdn\"			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)

libevolution_module_mdn_la_SOURCES =				\
	evolution-mdn.c

libevolution_module_mdn_la_LIBADD =				\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

libevolution_module_mdn_la_LDFLAGS =				\
	-module -avoid-version $(NO_UNDEFINED)

error_DATA = evolution-mdn.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = evolution-mdn.error.xml

-include $(top_srcdir)/git.mk

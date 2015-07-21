@EVO_PLUGIN_RULE@

module_LTLIBRARIES = module-itip-formatter.la

module_itip_formatter_la_CPPFLAGS =					\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-itip-formatter\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)						\
	$(NULL)

module_itip_formatter_la_SOURCES =					\
	e-mail-formatter-itip.c						\
	e-mail-formatter-itip.h						\
	e-mail-parser-itip.c						\
	e-mail-parser-itip.h						\
	e-mail-part-itip.c						\
	e-mail-part-itip.h						\
	itip-view.c							\
	itip-view.h							\
	evolution-module-itip-formatter.c

module_itip_formatter_la_LIBADD =					\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la		\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/shell/libevolution-shell.la			\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(top_builddir)/libemail-engine/libemail-engine.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(NULL)

module_itip_formatter_la_LDFLAGS =					\
	-avoid-version -module $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

error_DATA = org-gnome-itip-formatter.error
errordir = $(privdatadir)/errors

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-itip-formatter.error.xml

-include $(top_srcdir)/git.mk

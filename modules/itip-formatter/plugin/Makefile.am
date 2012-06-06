@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-itip-formatter.eplug
plugin_LTLIBRARIES = liborg-gnome-itip-formatter.la

liborg_gnome_itip_formatter_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/modules/itip-formatter		\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

liborg_gnome_itip_formatter_la_SOURCES =		\
	config-ui.c

liborg_gnome_itip_formatter_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_itip_formatter_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/em-format/libemformat.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/modules/itip-formatter/module-itip-formatter.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-itip-formatter.eplug.xml

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-itip-formatter.eplug
plugin_LTLIBRARIES = liborg-gnome-itip-formatter.la

liborg_gnome_itip_formatter_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/modules/itip-formatter		\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_itip_formatter_la_SOURCES =		\
	config-ui.c					\
	../e-conflict-search-selector.c			\
	../e-conflict-search-selector.h			\
	../e-source-conflict-search.c			\
	../e-source-conflict-search.h

liborg_gnome_itip_formatter_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_itip_formatter_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/em-format/libevolution-mail-formatter.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-itip-formatter.eplug.xml

-include $(top_srcdir)/git.mk

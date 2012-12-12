@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-prefer-plain.eplug
plugin_LTLIBRARIES = liborg-gnome-prefer-plain.la

liborg_gnome_prefer_plain_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_prefer_plain_la_SOURCES =		\
	config-ui.c

liborg_gnome_prefer_plain_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_prefer_plain_la_LIBADD =				\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-prefer-plain.eplug.xml

-include $(top_srcdir)/git.mk

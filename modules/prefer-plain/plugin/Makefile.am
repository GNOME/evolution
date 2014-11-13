@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-prefer-plain.eplug
plugin_LTLIBRARIES = liborg-gnome-prefer-plain.la

liborg_gnome_prefer_plain_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_prefer_plain_la_SOURCES =		\
	config-ui.c

liborg_gnome_prefer_plain_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

liborg_gnome_prefer_plain_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-prefer-plain.eplug.xml

-include $(top_srcdir)/git.mk

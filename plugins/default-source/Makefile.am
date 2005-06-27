INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)          \
	$(SOURCE_SEL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-default-source.eplug
plugin_LTLIBRARIES = liborg-gnome-default-source.la


liborg_gnome_default_source_la_SOURCES = default-source.c
liborg_gnome_default_source_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_default_source_la_LIBADD =		\
	$(EVOLUTION_CALENDAR_LIBS)		\
	$(EVOLUTION_ADDRESSBOOK_LIBS)


EXTRA_DIST = org-gnome-default-source.eplug.in

INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-select-one-source.eplug
plugin_LTLIBRARIES = liborg-gnome-select-one-source.la

liborg_gnome_select_one_source_la_SOURCES = select-one-source.c
liborg_gnome_select_one_source_la_LDFLAGS = -module -avoid-version

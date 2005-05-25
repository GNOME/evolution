INCLUDES =						\
	-I$(top_srcdir)					\
	$(E_UTIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-mono.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-mono.la

liborg_gnome_evolution_mono_la_SOURCES = mono-plugin.c
liborg_gnome_evolution_mono_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-evolution-mono.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

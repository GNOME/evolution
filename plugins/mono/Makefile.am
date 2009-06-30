AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	$(E_UTIL_CFLAGS)				\
	$(MONO_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-mono.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-mono.la

liborg_gnome_evolution_mono_la_SOURCES = mono-plugin.c mono-plugin.h
liborg_gnome_evolution_mono_la_LDFLAGS = -module -avoid-version
liborg_gnome_evolution_mono_la_LIBADD =	\
	$(E_UTIL_LIBS)			\
	$(MONO_LIBS)

EXTRA_DIST = org-gnome-evolution-mono.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

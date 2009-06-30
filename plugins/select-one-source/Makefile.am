AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-select-one-source.eplug
plugin_LTLIBRARIES = liborg-gnome-select-one-source.la

liborg_gnome_select_one_source_la_SOURCES = select-one-source.c
liborg_gnome_select_one_source_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_select_one_source_la_LIBADD =	\
	$(EVOLUTION_CALENDAR_LIBS)

EXTRA_DIST = org-gnome-select-one-source.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

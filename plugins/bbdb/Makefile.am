INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-bbdb.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-bbdb.la

liborg_gnome_evolution_bbdb_la_SOURCES = bbdb.c bbdb.h gaimbuddies.c
liborg_gnome_evolution_bbdb_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-evolution-bbdb.eplug.in
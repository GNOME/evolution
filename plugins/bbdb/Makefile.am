INCLUDES =						\
	-I$(top_builddir)/composer			\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-bbdb.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-bbdb.la

liborg_gnome_evolution_bbdb_la_SOURCES = bbdb.c bbdb.h gaimbuddies.c
liborg_gnome_evolution_bbdb_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_evolution_bbdb_la_LIBADD =		\
	$(top_builddir)/e-util/libeutil.la	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

EXTRA_DIST = org-gnome-evolution-bbdb.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-bbdb.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-bbdb.la

liborg_gnome_evolution_bbdb_la_SOURCES = bbdb.c gaimbuddies.c
liborg_gnome_evolution_bbdb_la_LDFLAGS = -module -avoid-version

# noinst_PROGRAMS = test-evobuddy

test_evobuddy_LDADD = $(top_builddir)/camel/libcamel.la $(EVOLUTION_ADDRESSBOOK_LIBS) $(EVOLUTION_MAIL_LIBS) $(CAMEL_LIBS)
test_evobuddy_SOURCES = test-evobuddy.c gaimbuddies.c bbdb.c
test_evobuddy_INCLUDES = $(INCLUDES) 

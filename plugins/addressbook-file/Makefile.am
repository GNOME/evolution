INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)		


@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-addressbook-file.eplug
plugin_LTLIBRARIES = liborg-gnome-addressbook-file.la


liborg_gnome_addressbook_file_la_SOURCES = addressbook-file.c
liborg_gnome_addressbook_file_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_addressbook_file_la_LIBADD =	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

EXTRA_DIST = org-gnome-addressbook-file.eplug.in

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)		


@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-addressbook-file.eplug
plugin_LTLIBRARIES = liborg-gnome-addressbook-file.la


liborg_gnome_addressbook_file_la_SOURCES = addressbook-file.c
liborg_gnome_addressbook_file_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-addressbook-file.eplug.in

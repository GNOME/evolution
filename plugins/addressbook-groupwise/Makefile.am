INCLUDES =					\
	-I$(top_srcdir)				\
	$(EVOLUTION_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)		


@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-addressbook-groupwise.eplug
plugin_LTLIBRARIES = liborg-gnome-addressbook-groupwise.la


liborg_gnome_addressbook_groupwise_la_SOURCES = addressbook-groupwise.c
liborg_gnome_addressbook_groupwise_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-addressbook-groupwise.eplug.in

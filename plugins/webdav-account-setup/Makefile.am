AM_CPPFLAGS = \
	-I .				\
	-I$(top_srcdir)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-webdav.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-webdav.la

liborg_gnome_evolution_webdav_la_SOURCES = \
	webdav-contacts-source.c

liborg_gnome_evolution_webdav_la_LIBADD = \
	$(EPLUGIN_LIBS)	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)	

liborg_gnome_evolution_webdav_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST = \
	org-gnome-evolution-webdav.eplug.xml

CLEANFILES = org-gnome-evolution-webdav.eplug

-include $(top_srcdir)/git.mk

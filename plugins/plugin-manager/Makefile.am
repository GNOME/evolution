INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-plugin-manager.eplug org-gnome-plugin-manager.xml
plugin_LTLIBRARIES = liborg-gnome-plugin-manager.la

liborg_gnome_plugin_manager_la_SOURCES = plugin-manager.c
liborg_gnome_plugin_manager_la_LDFLAGS = -module -avoid-version

EXTRA_DIST =						\
	org-gnome-plugin-manager.eplug.xml		\
	org-gnome-plugin-manager.xml

BUILT_SOURCES = org-gnome-plugin-manager.eplug
CLEANFILES = $(BUILT_SOURCES)

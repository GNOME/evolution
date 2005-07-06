INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-sa-junk-plugin.eplug
plugin_LTLIBRARIES = liborg-gnome-sa-junk-plugin.la

liborg_gnome_sa_junk_plugin_la_SOURCES = em-junk-filter.c
liborg_gnome_sa_junk_plugin_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)


BUILT_SOURCES = $(plugin_DATA) $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-sa-junk-plugin.eplug.xml



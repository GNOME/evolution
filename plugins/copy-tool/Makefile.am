INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-copy-tool.eplug
plugin_LTLIBRARIES = liborg-gnome-copy-tool.la

liborg_gnome_copy_tool_la_SOURCES = copy-tool.c
liborg_gnome_copy_tool_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-copy-tool.eplug.in
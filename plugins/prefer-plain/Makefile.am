INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-prefer-plain.eplug
plugin_LTLIBRARIES = liborg-gnome-prefer-plain.la

liborg_gnome_prefer_plain_la_SOURCES = prefer-plain.c
liborg_gnome_prefer_plain_la_LDFLAGS = -module -avoid-version

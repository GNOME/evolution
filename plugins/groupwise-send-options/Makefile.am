INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-compose-send-options.eplug org-gnome-compose-send-options.xml
plugin_LTLIBRARIES = liborg-gnome-compose-send-options.la

liborg_gnome_compose_send_options_la_SOURCES = send-options.c send-options.h
liborg_gnome_compose_send_options_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = 					\
	org-gnome-compose-send-options.eplug.in	\
	org-gnome-compose-send-options.xml

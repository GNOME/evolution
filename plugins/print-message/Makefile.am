INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-print-message.eplug org-gnome-print-message.xml
plugin_LTLIBRARIES = liborg-gnome-print-message.la

liborg_gnome_print_message_la_SOURCES = print-message.c
liborg_gnome_print_message_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = 					\
	org-gnome-compose-print-message.eplug.xml\
	org-gnome-compose-print-message.xml
CLEANFILES = $(BUILT_SOURCES)

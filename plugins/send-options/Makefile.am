INCLUDES =						\
	-I$(top_srcdir)\
	-I$(top_srcdir)/camel                           \
		$(EVOLUTION_MAIL_CFLAGS)\
		$(CAMEL_GROUPWISE_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-send-options.eplug
plugin_LTLIBRARIES = liborg-gnome-send-options.la

liborg_gnome_send_options_la_SOURCES = send-options.c
liborg_gnome_send_options_la_LDFLAGS = -module -avoid-version
liborg_gnome_send_options_la_LIBADD= $(prefix)/lib/libegroupwise-1.2.la

EXTRA_DIST = org-gnome-send-options.eplug.in 

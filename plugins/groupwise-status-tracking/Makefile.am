INCLUDES =						\
	-I$(top_srcdir)					\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-status-track.eplug
plugin_LTLIBRARIES = liborg-gnome-status-track.la

liborg_gnome_status_track_la_SOURCES = status-track.c
liborg_gnome_status_track_la_LIBADD = $(CAMEL_GROUPWISE_LIBS)
liborg_gnome_status_track_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-status-track.eplug.in

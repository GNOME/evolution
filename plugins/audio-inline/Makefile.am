INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(GSTREAMER_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-audio-inline.eplug
plugin_LTLIBRARIES = liborg-gnome-audio-inline.la

liborg_gnome_audio_inline_la_SOURCES = audio-inline.c
liborg_gnome_audio_inline_la_LDFLAGS = -module -avoid-version
liborg_gnome_audio_inline_la_LIBADD = $(GSTREAMER_LIBS)

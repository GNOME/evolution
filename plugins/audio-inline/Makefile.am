if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(GNOME_PLATFORM_LIBS)
endif

AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(GSTREAMER_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-audio-inline.eplug
plugin_LTLIBRARIES = liborg-gnome-audio-inline.la

liborg_gnome_audio_inline_la_SOURCES = audio-inline.c
liborg_gnome_audio_inline_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_audio_inline_la_LIBADD =	\
	$(GSTREAMER_LIBS)		\
	$(EVOLUTION_MAIL_LIBS)			\
	$(NO_UNDEFINED_REQUIRED_LIBS)

EXTRA_DIST = org-gnome-audio-inline.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

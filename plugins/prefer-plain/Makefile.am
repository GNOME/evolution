@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-prefer-plain.eplug

plugin_LTLIBRARIES = liborg-gnome-prefer-plain.la

liborg_gnome_prefer_plain_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

liborg_gnome_prefer_plain_la_SOURCES = prefer-plain.c

liborg_gnome_prefer_plain_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_prefer_plain_la_LIBADD =	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/em-format/libemformat.la	\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = org-gnome-prefer-plain.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

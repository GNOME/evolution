@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-default-source.eplug

plugin_LTLIBRARIES = liborg-gnome-default-source.la

liborg_gnome_default_source_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)		\
	$(GNOME_PLATFORM_CFLAGS)

liborg_gnome_default_source_la_SOURCES = default-source.c

liborg_gnome_default_source_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_default_source_la_LIBADD =		\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/shell/libeshell.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = org-gnome-default-source.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

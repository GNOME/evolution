@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-contacts-map.eplug
plugin_LTLIBRARIES = liborg-gnome-contacts-map.la

liborg_gnome_contacts_map_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CHAMPLAIN_CFLAGS)				\
	$(GEOCLUE_CFLAGS)

liborg_gnome_contacts_map_la_SOURCES = \
		contacts-map.c \
		geo-utils.c \
		geo-utils.h

liborg_gnome_contacts_map_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_contacts_map_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/shell/libeshell.la	\
	$(CHAMPLAIN_LIBS)		\
	$(GEOCLUE_LIBS)			\
	$(EVOLUTION_ADDRESSBOOK_LIBS)	\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = org-gnome-contacts-map.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

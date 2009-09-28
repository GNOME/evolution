@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-google.eplug

plugin_LTLIBRARIES = liborg-gnome-evolution-google.la

liborg_gnome_evolution_google_la_CPPFLAGS = 		\
	$(AM_CPPFLAGS)					\
	-I .						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-DCALDAV_UIDIR=\""$(uidir)"\"			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(GDATA_CFLAGS)

liborg_gnome_evolution_google_la_SOURCES =        \
	google-source.c \
	google-contacts-source.h \
	google-contacts-source.c

liborg_gnome_evolution_google_la_LIBADD = 			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/e-util/libeutil.la			\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(EPLUGIN_LIBS)						\
	$(GNOME_PLATFORM_LIBS)					\
	$(GDATA_LIBS)

liborg_gnome_evolution_google_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST =				\
	org-gnome-evolution-google.eplug.xml

CLEANFILES = org-gnome-evolution-google.eplug

-include $(top_srcdir)/git.mk

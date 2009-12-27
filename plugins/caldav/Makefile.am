@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-caldav.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-caldav.la

liborg_gnome_evolution_caldav_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)				\
	-I .					\
	-I$(top_srcdir)				\
	-DCALDAV_UIDIR=\""$(uidir)"\"		\
	$(EVOLUTION_CALENDAR_CFLAGS)		\
	$(LIBSOUP_CFLAGS)

liborg_gnome_evolution_caldav_la_SOURCES =	\
	caldav-source.c				\
	caldav-browse-server.h			\
	caldav-browse-server.c

liborg_gnome_evolution_caldav_la_LIBADD = 	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(EVOLUTION_CALENDAR_LIBS)		\
	$(EPLUGIN_LIBS)				\
	$(LIBSOUP_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

liborg_gnome_evolution_caldav_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST =				\
	org-gnome-evolution-caldav.eplug.xml

CLEANFILES = org-gnome-evolution-caldav.eplug

-include $(top_srcdir)/git.mk

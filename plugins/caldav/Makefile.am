AM_CPPFLAGS = 						\
	-I .						\
	-I$(top_srcdir)	                                \
	-DCALDAV_GLADEDIR=\""$(gladedir)"\"		\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-caldav.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-caldav.la

liborg_gnome_evolution_caldav_la_SOURCES =        \
	caldav-source.c

liborg_gnome_evolution_caldav_la_LIBADD = 	\
	$(EVOLUTION_CALENDAR_LIBS)		\
	$(EPLUGIN_LIBS)		

liborg_gnome_evolution_caldav_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST =				\
	org-gnome-evolution-caldav.eplug.xml

CLEANFILES = org-gnome-evolution-caldav.eplug

-include $(top_srcdir)/git.mk

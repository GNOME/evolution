INCLUDES = 						\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	-I .						\
	-I$(top_srcdir)	                                \
	-DCALDAV_GLADEDIR=\""$(gladedir)"\"		

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-google.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-google.la

liborg_gnome_evolution_google_la_SOURCES =        \
	google-source.c

liborg_gnome_evolution_google_la_LIBADD = 	\
	$(EVOLUTION_CALENDAR_LIB1S)		\
	$(EPLUGIN_LIBS)		

liborg_gnome_evolution_google_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST =				\
	org-gnome-evolution-google.eplug.xml

CLEANFILES = org-gnome-evolution-google.eplug

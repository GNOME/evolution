INCLUDES = -I .						\
	-I$(top_srcdir)	                                \
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-startup-wizard.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-startup-wizard.la

liborg_gnome_evolution_startup_wizard_la_SOURCES = 	\
	startup-wizard.c

liborg_gnome_evolution_startup_wizard_la_LIBADD = 		\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(EVOLUTION_MAIL_LIBS)

liborg_gnome_evolution_startup_wizard_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-evolution-startup-wizard.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

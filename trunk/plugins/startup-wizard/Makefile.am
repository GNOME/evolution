INCLUDES = -I .						\
	-I$(top_srcdir)	                                \
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-startup-wizard.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-startup-wizard.la

liborg_gnome_evolution_startup_wizard_la_SOURCES = 	\
	startup-wizard.c

liborg_gnome_evolution_startup_wizard_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la		\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(EVOLUTION_CALENDAR_LIBS)					\
	$(EVOLUTION_MAIL_LIBS)

liborg_gnome_evolution_startup_wizard_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST = org-gnome-evolution-startup-wizard.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

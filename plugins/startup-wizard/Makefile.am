plugin_LTLIBRARIES = liborg-gnome-evolution-startup-wizard.la

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-startup-wizard.eplug

liborg_gnome_evolution_startup_wizard_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)					\
	-I .						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)/shell				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

liborg_gnome_evolution_startup_wizard_la_SOURCES = 	\
	startup-wizard.c

liborg_gnome_evolution_startup_wizard_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la		\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/capplet/libevolution-mail-settings.la		\
	$(EVOLUTION_CALENDAR_LIBS)					\
	$(EVOLUTION_MAIL_LIBS)						\
	$(GNOME_PLATFORM_LIBS)

liborg_gnome_evolution_startup_wizard_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST = org-gnome-evolution-startup-wizard.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

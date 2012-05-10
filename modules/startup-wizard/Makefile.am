module_LTLIBRARIES = module-startup-wizard.la

module_startup_wizard_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-startup-wizard\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

module_startup_wizard_la_SOURCES = \
	evolution-startup-wizard.c

module_startup_wizard_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la \
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/capplet/settings/libevolution-mail-settings.la \
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(top_builddir)/libemail-utils/libemail-utils.la  \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

module_startup_wizard_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

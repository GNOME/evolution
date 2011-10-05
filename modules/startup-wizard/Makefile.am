module_LTLIBRARIES = libevolution-module-startup-wizard.la

libevolution_module_startup_wizard_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-startup-wizard\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

libevolution_module_startup_wizard_la_SOURCES =			\
	evolution-startup-wizard.c

libevolution_module_startup_wizard_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la \
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/capplet/settings/libevolution-mail-settings.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_startup_wizard_la_LDFLAGS =			\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

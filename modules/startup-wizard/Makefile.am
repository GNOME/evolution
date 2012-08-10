NULL =

module_LTLIBRARIES = module-startup-wizard.la

module_startup_wizard_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-startup-wizard\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(NULL)

module_startup_wizard_la_SOURCES = \
	evolution-startup-wizard.c				\
	e-startup-assistant.c					\
	e-startup-assistant.h					\
	e-mail-config-import-page.c				\
	e-mail-config-import-page.h				\
	e-mail-config-import-progress-page.c			\
	e-mail-config-import-progress-page.h			\
	$(NULL)

module_startup_wizard_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la \
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(top_builddir)/libemail-utils/libemail-utils.la	\
	$(libevolution_mail_settings_la)			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

module_startup_wizard_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

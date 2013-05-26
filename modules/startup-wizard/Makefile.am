NULL =

module_LTLIBRARIES = module-startup-wizard.la

module_startup_wizard_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-startup-wizard\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
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
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(libevolution_mail_settings_la)			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(NULL)

module_startup_wizard_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

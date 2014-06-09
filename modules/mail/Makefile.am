module_LTLIBRARIES = module-mail.la

module_mail_la_CPPFLAGS = \
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-mail\"			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)						\
	$(NULL)

module_mail_la_SOURCES = \
	evolution-module-mail.c						\
	e-mail-attachment-handler.c					\
	e-mail-attachment-handler.h					\
	e-mail-config-hook.c						\
	e-mail-config-hook.h						\
	e-mail-event-hook.c						\
	e-mail-event-hook.h						\
	e-mail-shell-backend.c						\
	e-mail-shell-backend.h						\
	e-mail-shell-content.c						\
	e-mail-shell-content.h						\
	e-mail-shell-sidebar.c						\
	e-mail-shell-sidebar.h						\
	e-mail-shell-view.c						\
	e-mail-shell-view.h						\
	e-mail-shell-view-actions.c					\
	e-mail-shell-view-actions.h					\
	e-mail-shell-view-private.c					\
	e-mail-shell-view-private.h					\
	em-account-prefs.c						\
	em-account-prefs.h						\
	em-composer-prefs.c						\
	em-composer-prefs.h						\
	em-mailer-prefs.c						\
	em-mailer-prefs.h						\
	$(NULL)

module_mail_la_LIBADD = \
	$(top_builddir)/libemail-engine/libemail-engine.la		\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(top_builddir)/shell/libevolution-shell.la			\
	$(top_builddir)/composer/libevolution-mail-composer.la		\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/mail/importers/libevolution-mail-importers.la	\
	$(libevolution_mail_settings_la)				\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(NULL)

module_mail_la_LDFLAGS = \
	-avoid-version -module $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

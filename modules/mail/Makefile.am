INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/mail						\
	-I$(top_srcdir)/widgets						\
	$(EVOLUTION_MAIL_CFLAGS)					\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-mail\"

module_LTLIBRARIES = libevolution-module-mail.la

libevolution_module_mail_la_SOURCES =					\
	evolution-module-mail.c						\
	e-attachment-handler-mail.c					\
	e-attachment-handler-mail.h					\
	e-mail-shell-backend.c						\
	e-mail-shell-backend.h						\
	e-mail-shell-content.c						\
	e-mail-shell-content.h						\
	e-mail-shell-migrate.c						\
	e-mail-shell-migrate.h						\
	e-mail-shell-settings.c						\
	e-mail-shell-settings.h						\
	e-mail-shell-sidebar.c						\
	e-mail-shell-sidebar.h						\
	e-mail-shell-view.c						\
	e-mail-shell-view.h						\
	e-mail-shell-view-actions.c					\
	e-mail-shell-view-actions.h					\
	e-mail-shell-view-private.c					\
	e-mail-shell-view-private.h					\
	em-account-editor.c						\
	em-account-editor.h						\
	em-account-prefs.c						\
	em-account-prefs.h						\
	em-composer-prefs.c						\
	em-composer-prefs.h						\
	em-mailer-prefs.c						\
	em-mailer-prefs.h						\
	em-network-prefs.c						\
	em-network-prefs.h

libevolution_module_mail_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/composer/libcomposer.la				\
	$(top_builddir)/widgets/table/libetable.la			\
	$(top_builddir)/widgets/text/libetext.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/mail/importers/libevolution-mail-importers.la

libevolution_module_mail_la_LDFLAGS =					\
	-avoid-version -module $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

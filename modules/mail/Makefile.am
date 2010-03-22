module_LTLIBRARIES = libevolution-module-mail.la

libevolution_module_mail_la_CPPFLAGS =					\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/mail						\
	-I$(top_srcdir)/widgets						\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(EVOLUTION_MAIL_CFLAGS)					\
	$(CAMEL_CFLAGS)							\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-mail\"

libevolution_module_mail_la_SOURCES =					\
	evolution-module-mail.c						\
	e-mail-attachment-handler.c					\
	e-mail-attachment-handler.h					\
	e-mail-config-format-html.c					\
	e-mail-config-format-html.h					\
	e-mail-config-hook.c						\
	e-mail-config-hook.h						\
	e-mail-config-web-view.c					\
	e-mail-config-web-view.h					\
	e-mail-event-hook.c						\
	e-mail-event-hook.h						\
	e-mail-junk-hook.c						\
	e-mail-junk-hook.h						\
	e-mail-shell-backend.c						\
	e-mail-shell-backend.h						\
	e-mail-shell-content.c						\
	e-mail-shell-content.h						\
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
	$(top_builddir)/em-format/libemformat.la			\
	$(top_builddir)/filter/libfilter.la				\
	$(top_builddir)/shell/libeshell.la				\
	$(top_builddir)/composer/libcomposer.la				\
	$(top_builddir)/widgets/table/libetable.la			\
	$(top_builddir)/widgets/text/libetext.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/mail/importers/libevolution-mail-importers.la	\
	$(top_builddir)/em-format/libemformat.la			\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/filter/libfilter.la				\
	$(top_builddir)/capplet/settings/libevolution-mail-settings.la	\
	$(EVOLUTION_MAIL_LIBS)						\
	$(CAMEL_LIBS)							\
	$(GTKHTML_LIBS)							\
	$(E_WIDGETS_LIBS)						\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_mail_la_LDFLAGS =					\
	-avoid-version -module $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

SUBDIRS = . default importers

privsolib_LTLIBRARIES = libevolution-mail.la

ENUM_TYPES = e-mail-enums.h

e-mail-enumtypes.h: $(top_srcdir)/enumtypes.h.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.h.template \
		--fhead "#ifndef E_MAIL_ENUMTYPES_H\n#define E_MAIL_ENUMTYPES_H\n" \
		--ftail "#endif /* E_MAIL_ENUMTYPES_H */\n" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

e-mail-enumtypes.c: $(top_srcdir)/enumtypes.c.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.c.template \
		--fhead "#include \"e-mail-enumtypes.h\"" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

ENUM_GENERATED = e-mail-enumtypes.h e-mail-enumtypes.c

mailincludedir = $(privincludedir)/mail

libevolution_mail_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/em-format			\
	-I$(top_srcdir)/mail				\
	-I$(top_srcdir)/composer			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_ICONDIR=\""$(icondir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_BUTTONSDIR=\""$(buttonsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DCAMEL_PROVIDERDIR=\""$(camel_providerdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DG_LOG_DOMAIN=\"evolution-mail\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CERT_UI_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(LIBCRYPTUI_CFLAGS)				\
	$(LIBSOUP_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

mailinclude_HEADERS =					\
	e-http-request.h				\
	e-mail.h					\
	e-mail-account-manager.h			\
	e-mail-account-store.h				\
	e-mail-account-tree-view.h			\
	e-mail-autoconfig.h				\
	e-mail-backend.h				\
	e-mail-browser.h				\
	e-mail-config-activity-page.h			\
	e-mail-config-assistant.h			\
	e-mail-config-auth-check.h			\
	e-mail-config-confirm-page.h			\
	e-mail-config-defaults-page.h			\
	e-mail-config-identity-page.h			\
	e-mail-config-lookup-page.h			\
	e-mail-config-notebook.h			\
	e-mail-config-page.h				\
	e-mail-config-provider-page.h			\
	e-mail-config-receiving-page.h			\
	e-mail-config-security-page.h			\
	e-mail-config-sending-page.h			\
	e-mail-config-service-backend.h			\
	e-mail-config-service-notebook.h		\
	e-mail-config-service-page.h			\
	e-mail-config-sidebar.h				\
	e-mail-config-summary-page.h			\
	e-mail-config-welcome-page.h			\
	e-mail-config-window.h				\
	e-mail-display.h				\
	e-mail-display-popup-extension.h		\
	e-mail-enums.h					\
	e-mail-enumtypes.h				\
	e-mail-folder-create-dialog.h			\
	e-mail-folder-pane.h				\
	e-mail-free-form-exp.h				\
	e-mail-junk-options.h				\
	e-mail-label-action.h				\
	e-mail-label-dialog.h				\
	e-mail-label-list-store.h			\
	e-mail-label-manager.h				\
	e-mail-label-tree-view.h			\
	e-mail-message-pane.h				\
	e-mail-migrate.h				\
	e-mail-notes.h					\
	e-mail-paned-view.h				\
	e-mail-print-config-headers.h			\
	e-mail-printer.h				\
	e-mail-properties.h				\
	e-mail-reader-utils.h				\
	e-mail-reader.h					\
	e-mail-remote-content.h				\
	e-mail-request.h				\
	e-mail-send-account-override.h			\
	e-mail-sidebar.h				\
	e-mail-tag-editor.h				\
	e-mail-ui-session.h				\
	e-mail-view.h					\
	em-composer-utils.h				\
	em-config.h					\
	em-event.h					\
	em-filter-context.h				\
	em-filter-editor.h				\
	em-filter-editor-folder-element.h		\
	em-filter-rule.h				\
	em-filter-source-element.h			\
	em-folder-properties.h				\
	em-folder-selection-button.h			\
	em-folder-selector.h				\
	em-folder-tree-model.h				\
	em-folder-tree.h				\
	em-folder-utils.h				\
	em-search-context.h				\
	em-subscription-editor.h			\
	em-utils.h					\
	em-vfolder-editor-context.h			\
	em-vfolder-editor-rule.h			\
	em-vfolder-editor.h				\
	mail-autofilter.h				\
	mail-send-recv.h				\
	mail-vfolder-ui.h				\
	message-list.h

libevolution_mail_la_SOURCES =				\
	e-http-request.c				\
	e-mail-account-manager.c			\
	e-mail-account-store.c				\
	e-mail-account-tree-view.c			\
	e-mail-autoconfig.c				\
	e-mail-backend.c				\
	e-mail-browser.c				\
	e-mail-config-activity-page.c			\
	e-mail-config-assistant.c			\
	e-mail-config-auth-check.c			\
	e-mail-config-confirm-page.c			\
	e-mail-config-defaults-page.c			\
	e-mail-config-identity-page.c			\
	e-mail-config-lookup-page.c			\
	e-mail-config-notebook.c			\
	e-mail-config-page.c				\
	e-mail-config-provider-page.c			\
	e-mail-config-receiving-page.c			\
	e-mail-config-security-page.c			\
	e-mail-config-sending-page.c			\
	e-mail-config-service-backend.c			\
	e-mail-config-service-notebook.c		\
	e-mail-config-service-page.c			\
	e-mail-config-sidebar.c				\
	e-mail-config-summary-page.c			\
	e-mail-config-welcome-page.c			\
	e-mail-config-window.c				\
	e-mail-display.c				\
	e-mail-display-popup-extension.c		\
	e-mail-enumtypes.c				\
	e-mail-folder-create-dialog.c			\
	e-mail-folder-pane.c				\
	e-mail-free-form-exp.c				\
	e-mail-junk-options.c				\
	e-mail-label-action.c				\
	e-mail-label-dialog.c				\
	e-mail-label-list-store.c			\
	e-mail-label-manager.c				\
	e-mail-label-tree-view.c			\
	e-mail-message-pane.c				\
	e-mail-migrate.c				\
	e-mail-notes.c					\
	e-mail-paned-view.c				\
	e-mail-print-config-headers.c			\
	e-mail-printer.c				\
	e-mail-properties.c				\
	e-mail-reader-utils.c				\
	e-mail-reader.c					\
	e-mail-remote-content.c				\
	e-mail-request.c				\
	e-mail-send-account-override.c			\
	e-mail-sidebar.c				\
	e-mail-tag-editor.c				\
	e-mail-ui-session.c				\
	e-mail-view.c					\
	em-composer-utils.c				\
	em-config.c					\
	em-event.c					\
	em-filter-context.c				\
	em-filter-editor.c				\
	em-filter-editor-folder-element.c		\
	em-filter-rule.c				\
	em-filter-source-element.c			\
	em-folder-properties.c				\
	em-folder-selection-button.c			\
	em-folder-selector.c				\
	em-folder-tree-model.c				\
	em-folder-tree.c				\
	em-folder-utils.c				\
	em-search-context.c				\
	em-subscription-editor.c			\
	em-utils.c					\
	em-vfolder-editor-context.c			\
	em-vfolder-editor-rule.c			\
	em-vfolder-editor.c				\
	mail-autofilter.c				\
	mail-send-recv.c				\
	mail-vfolder-ui.c				\
	message-list.c

if ENABLE_SMIME
SMIME_LIBS =						\
	$(top_builddir)/smime/lib/libessmime.la		\
	$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_mail_la_LIBADD =				\
	$(top_builddir)/libemail-engine/libemail-engine.la		\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/shell/libevolution-shell.la	\
	$(top_builddir)/composer/libevolution-mail-composer.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/em-format/libevolution-mail-formatter.la \
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(CERT_UI_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(SMIME_LIBS)					\
	$(LIBCRYPTUI_LIBS)				\
	$(LIBSOUP_LIBS)					\
	$(NULL)

libevolution_mail_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

noinst_PROGRAMS = test-mail-autoconfig

test_mail_autoconfig_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

test_mail_autoconfig_SOURCES = \
	e-mail-autoconfig.c				\
	e-mail-autoconfig.h				\
	test-mail-autoconfig.c

test_mail_autoconfig_LDADD = \
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

# Misc data to install
filterdir = $(privdatadir)
filter_in_files = \
	filtertypes.xml.in \
	searchtypes.xml.in \
	vfoldertypes.xml.in \
	$(NULL)
filter_DATA = $(filter_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

error_DATA = mail.error
errordir = $(privdatadir)/errors
# provides error rules too
@EVO_PLUGIN_RULE@

ui_DATA = mail-config.ui mail-dialogs.ui

etspec_DATA = message-list.etspec

EXTRA_DIST = 				\
	README.async			\
	mail.error.xml			\
	$(ui_DATA)			\
	$(server_in_files)		\
	$(etspec_DATA)			\
	$(filter_in_files)		\
	$(NULL)

# Purify support

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif

# Prologue

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(ENUM_GENERATED) $(error_DATA) $(filter_DATA)

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

SUBDIRS = . default importers mail-autoconfig

privsolib_LTLIBRARIES = libevolution-mail.la

mailincludedir = $(privincludedir)/mail

include $(top_srcdir)/glib-gen.mak
glib_enum_headers=e-mail-enums.h
glib_enum_output=e-mail-enumtypes
glib_enum_define=E_MAIL
glib_enum_prefix=e_mail

ENUM_GENERATED = e-mail-enumtypes.h e-mail-enumtypes.c

libevolution_mail_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/em-format			\
	-I$(top_srcdir)/mail				\
	-I$(top_srcdir)/composer			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CERT_UI_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(CLUTTER_CFLAGS)				\
	$(GTKHTML_CFLAGS)				\
	$(EMAIL_UTILS_CFLAGS)				\
	$(EMAIL_ENGINE_CFLAGS)				\
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
	-DG_LOG_DOMAIN=\"evolution-mail\"

mailinclude_HEADERS =					\
	e-mail.h					\
	e-mail-account-manager.h			\
	e-mail-account-store.h				\
	e-mail-account-tree-view.h			\
	e-mail-attachment-bar.h				\
	e-mail-backend.h				\
	e-mail-browser.h				\
	e-mail-display.h				\
	e-mail-enums.h					\
	e-mail-enumtypes.h				\
	e-mail-folder-pane.h				\
	e-mail-folder-utils.h				\
	e-mail-junk-filter.h				\
	e-mail-junk-options.h				\
	e-mail-label-action.h				\
	e-mail-label-dialog.h				\
	e-mail-label-list-store.h			\
	e-mail-label-manager.h				\
	e-mail-label-tree-view.h			\
	e-mail-message-pane.h				\
	e-mail-migrate.h				\
	e-mail-notebook-view.h				\
	e-mail-paned-view.h				\
	e-mail-reader-utils.h				\
	e-mail-reader.h					\
	e-mail-session-utils.h				\
	e-mail-session.h				\
	e-mail-sidebar.h				\
	e-mail-store-utils.h				\
	e-mail-tag-editor.h				\
	e-mail-view.h					\
	em-account-editor.h				\
	em-composer-utils.h				\
	em-config.h					\
	em-event.h					\
	em-filter-context.h				\
	em-filter-editor.h				\
	em-filter-folder-element.h			\
	em-filter-rule.h				\
	em-filter-source-element.h			\
	em-folder-properties.h				\
	em-folder-selection-button.h			\
	em-folder-selector.h				\
	em-folder-tree-model.h				\
	em-folder-tree.h				\
	em-folder-utils.h				\
	em-format-hook.h				\
	em-format-html-display.h			\
	em-format-html-print.h				\
	em-format-html.h				\
	em-html-stream.h				\
	em-search-context.h				\
	em-subscription-editor.h			\
	em-sync-stream.h				\
	em-utils.h					\
	em-vfolder-context.h				\
	em-vfolder-editor.h				\
	em-vfolder-rule.h				\
	mail-autofilter.h				\
	mail-config.h					\
	mail-folder-cache.h				\
	mail-guess-servers.h				\
	mail-mt.h					\
	mail-ops.h					\
	mail-send-recv.h				\
	mail-tools.h					\
	mail-vfolder.h					\
	message-list.h

if ENABLE_CLUTTER
mailinclude_HEADERS +=					\
	e-mail-tab.h					\
	e-mail-tab-picker.h				

endif

libevolution_mail_la_SOURCES =				\
	e-mail-account-manager.c			\
	e-mail-account-store.c				\
	e-mail-account-tree-view.c			\
	e-mail-attachment-bar.c				\
	e-mail-backend.c				\
	e-mail-browser.c				\
	e-mail-display.c				\
	e-mail-enumtypes.c				\
	e-mail-folder-pane.c				\
	e-mail-folder-utils.c				\
	e-mail-junk-filter.c				\
	e-mail-junk-options.c				\
	e-mail-label-action.c				\
	e-mail-label-dialog.c				\
	e-mail-label-list-store.c			\
	e-mail-label-manager.c				\
	e-mail-label-tree-view.c			\
	e-mail-message-pane.c				\
	e-mail-migrate.c				\
	e-mail-notebook-view.c				\
	e-mail-paned-view.c				\
	e-mail-reader-utils.c				\
	e-mail-reader.c					\
	e-mail-sidebar.c				\
	e-mail-store-utils.c				\
	e-mail-tag-editor.c				\
	e-mail-view.c					\
	em-account-editor.c				\
	em-composer-utils.c				\
	em-config.c					\
	em-event.c					\
	em-filter-context.c				\
	em-filter-editor.c				\
	em-filter-folder-element.c			\
	em-filter-rule.c				\
	em-filter-source-element.c			\
	em-folder-properties.c				\
	em-folder-selection-button.c			\
	em-folder-selector.c				\
	em-folder-tree-model.c				\
	em-folder-tree.c				\
	em-folder-utils.c				\
	em-format-hook.c				\
	em-format-html-display.c			\
	em-format-html-print.c				\
	em-format-html.c				\
	em-html-stream.c				\
	em-search-context.c				\
	em-subscription-editor.c			\
	em-sync-stream.c				\
	em-utils.c					\
	em-vfolder-context.c				\
	em-vfolder-editor.c				\
	em-vfolder-rule.c				\
	mail-autofilter.c				\
	mail-config.c					\
	mail-guess-servers.c				\
	mail-send-recv.c				\
	mail-vfolder.c					\
	message-list.c

if ENABLE_CLUTTER
libevolution_mail_la_SOURCES +=				\
	e-mail-tab.c					\
	e-mail-tab-picker.c				
endif

if ENABLE_SMIME
SMIME_LIBS =						\
	$(top_builddir)/smime/lib/libessmime.la		\
	$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_mail_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/composer/libcomposer.la		\
	$(top_builddir)/widgets/table/libetable.la	\
	$(top_builddir)/widgets/text/libetext.la	\
	$(top_builddir)/widgets/menus/libmenus.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(top_builddir)/widgets/menus/libmenus.la	\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/em-format/libemformat.la	\
	$(top_builddir)/filter/libfilter.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(EMAIL_UTILS_LIBS)				\
	$(EMAIL_ENGINE_LIBS)				\
	$(CERT_UI_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(CLUTTER_LIBS)					\
	$(GTKHTML_LIBS)					\
	$(SMIME_LIBS)

libevolution_mail_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_mail_la_DEPENDENCIES = em-filter-i18n.h

# Misc data to install
filterdir = $(privdatadir)
filter_DATA = filtertypes.xml vfoldertypes.xml searchtypes.xml

error_DATA = mail.error
errordir = $(privdatadir)/errors
# provides error rules too
@EVO_PLUGIN_RULE@

em-filter-i18n.h: filtertypes.xml vfoldertypes.xml searchtypes.xml
	echo "/* Automatically generated. Do not edit. */" > $@;	      \
	cat $(srcdir)/filtertypes.xml $(srcdir)/vfoldertypes.xml $(srcdir)/searchtypes.xml |	      \
		sed -n -e 's:.*<title>\(.*\)</title>:gchar *s = N_("\1");:p' | \
		sort -u >> $@

ui_DATA = mail-config.ui mail-dialogs.ui

etspec_DATA = message-list.etspec

EXTRA_DIST = 				\
	README.async			\
	mail.error.xml			\
	$(ui_DATA)			\
	$(schema_in_files)		\
	$(server_in_files)		\
	$(etspec_DATA)			\
	filtertypes.xml			\
	vfoldertypes.xml		\
	searchtypes.xml			\
	em-filter-i18n.h

# Purify support

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif


# GConf

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = evolution-mail.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL

if OS_WIN32
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE);	\
			 echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat;	\
			cmd /c _temp.bat; 						\
			rm _temp.bat;							\
		done									\
	fi
else
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) 		\
				$(GCONFTOOL) --makefile-install-rule $$p;		\
		done									\
	fi
endif

endif

# Prologue

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(error_DATA) $(ENUM_GENERATED)

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk

SUBDIRS = default importers

mailincludedir = $(privincludedir)/mail

AM_CPPFLAGS =						\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/mail				\
	-I$(top_srcdir)/composer			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(CERT_UI_CFLAGS)				\
	$(THREADS_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"		\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_BUTTONSDIR=\""$(buttonsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(camel_providerdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DG_LOG_DOMAIN=\"evolution-mail\"

privsolib_LTLIBRARIES = libevolution-mail-shared.la
component_LTLIBRARIES = libevolution-mail.la

# Mail.idl
MAIL_IDL = Evolution-Mail.idl
MAIL_IDL_GENERATED_H = \
	Evolution-Mail.h
MAIL_IDL_GENERATED_C = \
	Evolution-Mail-common.c		\
	Evolution-Mail-skels.c		\
	Evolution-Mail-stubs.c
MAIL_IDL_GENERATED = $(MAIL_IDL_GENERATED_C) $(MAIL_IDL_GENERATED_H)

$(MAIL_IDL_GENERATED_H): $(MAIL_IDL)
	$(ORBIT_IDL) -I $(top_srcdir) -I $(datadir)/idl $(IDL_INCLUDES) $^
$(MAIL_IDL_GENERATED_C): $(MAIL_IDL_GENERATED_H)

# IDL install

idl_DATA = $(MAIL_IDL)

# plugin mail api
mailinclude_HEADERS =				\
	$(MAIL_IDL_GENERATED_H)			\
	e-mail-attachment-bar.h			\
	e-searching-tokenizer.h			\
	em-account-editor.h			\
	e-mail-search-bar.h			\
	em-composer-utils.h			\
	em-config.h				\
	em-event.h				\
	em-filter-context.h			\
	em-filter-editor.h			\
	em-filter-folder-element.h		\
	em-filter-rule.h			\
	em-filter-source-element.h		\
	em-folder-browser.h			\
	em-folder-tree-model.h			\
	em-folder-tree.h			\
	em-folder-utils.h			\
	em-folder-view.h			\
	em-format-hook.h			\
	em-format-html-display.h		\
	em-format-html-print.h			\
	em-format-html.h			\
	em-format-quote.h			\
	em-format.h				\
	em-html-stream.h			\
	em-icon-stream.h			\
	em-inline-filter.h			\
	em-junk-hook.h				\
	em-menu.h				\
	em-message-browser.h			\
	em-popup.h				\
	em-search-context.h			\
	em-stripsig-filter.h			\
	em-sync-stream.h			\
	em-utils.h				\
	mail-autofilter.h			\
	mail-component.h			\
	mail-config.h				\
	mail-folder-cache.h			\
	mail-mt.h				\
	mail-ops.h				\
	mail-send-recv.h			\
	mail-session.h				\
	mail-tools.h				\
	message-list.h				\
	mail-vfolder.h

# libevolution-mail

libevolution_mail_la_SOURCES =			\
	$(MAIL_IDL_GENERATED)			\
	$(mailinclude_HEADERS)			\
	e-searching-tokenizer.c			\
	em-account-prefs.c			\
	em-account-prefs.h			\
	em-composer-prefs.c			\
	em-composer-prefs.h			\
	em-folder-browser.c			\
	em-folder-view.c			\
	em-format-hook.c			\
	em-format-html-display.c		\
	em-format-html-print.c			\
	em-format-html.c			\
	em-html-stream.c			\
	em-junk-hook.c				\
	em-mailer-prefs.c			\
	em-mailer-prefs.h			\
	em-menu.c				\
	em-message-browser.c			\
	em-migrate.c				\
	em-migrate.h				\
	em-network-prefs.c			\
	em-network-prefs.h			\
	em-subscribe-editor.c			\
	em-subscribe-editor.h			\
	mail-component-factory.c		\
	mail-component.c			\
	mail-config-factory.c			\
	mail-config-factory.h			\
	mail-signature-editor.c			\
	mail-signature-editor.h			\
	mail-types.h				\
	message-list.c				
if ENABLE_SMIME
SMIME_LIBS = 				\
	$(top_builddir)/smime/lib/libessmime.la		\
	$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_mail_la_LIBADD =					\
	$(top_builddir)/mail/libevolution-mail-shared.la	\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/widgets/table/libetable.la		\
	$(top_builddir)/widgets/text/libetext.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/widgets/misc/libefilterbar.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/mail/importers/libevolution-mail-importers.la	\
	$(SMIME_LIBS)						\
	$(EVOLUTION_MAIL_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(REGEX_LIBS)						\
	$(THREADS_LIBS)

libevolution_mail_la_LDFLAGS = 			\
	-avoid-version -module $(NO_UNDEFINED)

libevolution_mail_la_DEPENDENCIES =		\
	em-filter-i18n.h			\
	libevolution-mail-shared.la

libevolution_mail_shared_la_SOURCES =		\
	e-attachment-handler-mail.c		\
	e-attachment-handler-mail.h		\
	e-mail-attachment-bar.c			\
	e-mail-search-bar.c			\
	em-account-editor.c			\
	em-account-editor.h			\
	em-event.c				\
	em-config.c				\
	em-composer-utils.c			\
	em-composer-utils.h			\
	em-filter-context.c			\
	em-filter-context.h			\
	em-filter-editor.c			\
	em-filter-editor.h			\
	em-filter-folder-element.c		\
	em-filter-folder-element.h		\
	em-filter-rule.c			\
	em-filter-rule.h			\
	em-filter-source-element.c		\
	em-filter-source-element.h		\
	em-folder-tree-model.c			\
	em-folder-tree.c			\
	em-folder-properties.c			\
	em-folder-properties.h			\
	em-folder-selection-button.c		\
	em-folder-selection-button.h		\
	em-folder-selection.c			\
	em-folder-selection.h			\
	em-folder-selector.c			\
	em-folder-selector.h			\
	em-folder-utils.c			\
	em-format-quote.c			\
	em-format.c				\
	em-icon-stream.c			\
	em-inline-filter.c			\
	em-popup.c				\
	em-search-context.c			\
	em-search-context.h			\
	em-stripsig-filter.c			\
	em-sync-stream.c			\
	em-utils.c				\
	em-vfolder-context.c			\
	em-vfolder-context.h			\
	em-vfolder-editor.c			\
	em-vfolder-editor.h			\
	em-vfolder-rule.c			\
	em-vfolder-rule.h			\
	mail-autofilter.c			\
	mail-config.c				\
	mail-crypto.c				\
	mail-crypto.h				\
	mail-folder-cache.c			\
	mail-folder-cache.h			\
	mail-ops.c				\
	mail-mt.c				\
	mail-send-recv.c			\
	mail-send-recv.h			\
	mail-session.c				\
	mail-tools.c				\
	mail-vfolder.c				\
	message-tag-editor.c			\
	message-tag-editor.h			\
	message-tag-followup.c			\
	message-tag-followup.h

if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libevolution-mail.la
endif

libevolution_mail_shared_la_LIBADD =				\
	$(WIN32_BOOTSTRAP_LIBS) \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(SMIME_LIBS)						\
	$(EVOLUTION_MAIL_LIBS)					\
	$(THREADS_LIBS)

libevolution_mail_shared_la_LDFLAGS = $(NO_UNDEFINED)

# --- BIG NASTY AUTOMAKE HACK ---
#
# Automake apparently sorts Makefile.am rules alphabetically.  And
# since component_LTLIBRARIES < privsolib_LTLIBRARIES, it tries to
# install libevolution-mail.la before libevolution-mail-shared.la,
# which fails when libtool tries to relink libevolution-mail.la
# against the not-yet-installed libevolution-mail-shared.la.
install-data-am:				\
	install-privsolibLTLIBRARIES		\
	install-componentLTLIBRARIES		\
	install-data-local			\
	install-errorDATA			\
	install-etspecDATA			\
	install-filterDATA			\
	install-gladeDATA			\
	install-idlDATA				\
	install-mailincludeHEADERS		\
	install-schemaDATA			\
	install-serverDATA

# .server files

server_in_files = GNOME_Evolution_Mail.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)

@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

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

glade_DATA = mail-config.glade mail-dialogs.glade

etspec_DATA = message-list.etspec

EXTRA_DIST = 				\
	ChangeLog.pre-1-4		\
	README.async			\
	$(MAIL_IDL)			\
	mail.error.xml			\
	$(glade_DATA)			\
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

if OS_WIN32
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE); \
			 echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat; \
			cmd /c _temp.bat; \
			rm _temp.bat; \
		done \
	fi
else
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi
endif

# Prologue

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(MAIL_IDL_GENERATED) $(server_DATA) $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk

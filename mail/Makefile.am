SUBDIRS = default importers

INCLUDES =						\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/composer			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/shell/importer			\
	-I$(top_builddir)/shell/importer		\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(CERT_UI_CFLAGS)				\
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

component_LTLIBRARIES = libevolution-mail.la

# Code generation for Spell.idl

SPELL_IDL = Spell.idl

SPELL_IDL_GENERATED_H = 		\
	Spell.h
SPELL_IDL_GENERATED_C = 		\
	Spell-common.c		\
	Spell-skels.c		\
	Spell-stubs.c
SPELL_IDL_GENERATED = $(SPELL_IDL_GENERATED_C) $(SPELL_IDL_GENERATED_H)

$(SPELL_IDL_GENERATED_H): $(SPELL_IDL)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) $(srcdir)/Spell.idl
$(SPELL_IDL_GENERATED_C): $(SPELL_IDL_GENERATED_H)

Spell-impl.o: Spell.h


# libevolution-mail

libevolution_mail_la_SOURCES =			\
	$(SPELL_IDL_GENERATED)			\
	$(MARSHAL_GENERATED)			\
	e-searching-tokenizer.c			\
	e-searching-tokenizer.h			\
	em-account-editor.c			\
	em-account-editor.h			\
	em-account-prefs.c			\
	em-account-prefs.h			\
	em-composer-prefs.c			\
	em-composer-prefs.h			\
	em-composer-utils.c			\
	em-composer-utils.h			\
	em-config.c				\
	em-config.h				\
	em-event.c				\
	em-event.h				\
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
	em-folder-browser.c			\
	em-folder-browser.h			\
	em-folder-properties.c			\
	em-folder-properties.h			\
	em-folder-selection-button.c		\
	em-folder-selection-button.h		\
	em-folder-selection.c			\
	em-folder-selection.h			\
	em-folder-selector.c			\
	em-folder-selector.h			\
	em-folder-tree-model.c			\
	em-folder-tree-model.h			\
	em-folder-tree.c			\
	em-folder-tree.h			\
	em-folder-view.c			\
	em-folder-view.h			\
	em-format-hook.c			\
	em-format-hook.h			\
	em-format-html-display.c		\
	em-format-html-display.h		\
	em-format-html-print.c			\
	em-format-html-print.h			\
	em-format-html.c			\
	em-format-html.h			\
	em-format-quote.c			\
	em-format-quote.h			\
	em-format.c				\
	em-format.h				\
	em-html-stream.c			\
	em-html-stream.h			\
	em-icon-stream.c			\
	em-icon-stream.h			\
	em-inline-filter.c			\
	em-inline-filter.h			\
	em-junk-filter.c			\
	em-junk-filter.h			\
	em-junk-plugin.c			\
	em-junk-plugin.h			\
	em-mailer-prefs.c			\
	em-mailer-prefs.h			\
	em-menu.c				\
	em-menu.h				\
	em-message-browser.c			\
	em-message-browser.h			\
	em-migrate.c				\
	em-migrate.h				\
	em-popup.c				\
	em-popup.h				\
	em-search-context.c			\
	em-search-context.h			\
	em-stripsig-filter.c			\
	em-stripsig-filter.h			\
	em-subscribe-editor.c			\
	em-subscribe-editor.h			\
	em-sync-stream.c			\
	em-sync-stream.h			\
	em-utils.c				\
	em-utils.h				\
	em-vfolder-context.c			\
	em-vfolder-context.h			\
	em-vfolder-editor.c			\
	em-vfolder-editor.h			\
	em-vfolder-rule.c			\
	em-vfolder-rule.h			\
	em-vfs-stream.c				\
	em-vfs-stream.h				\
	mail-autofilter.c			\
	mail-autofilter.h			\
	mail-component-factory.c		\
	mail-component.c			\
	mail-component.h			\
	mail-config-factory.c			\
	mail-config-factory.h			\
	mail-config.c				\
	mail-config.h				\
	mail-crypto.c				\
	mail-crypto.h				\
	mail-folder-cache.c			\
	mail-folder-cache.h			\
	mail-mt.c				\
	mail-mt.h				\
	mail-offline-handler.c			\
	mail-offline-handler.h			\
	mail-ops.c				\
	mail-ops.h				\
	mail-send-recv.c			\
	mail-send-recv.h			\
	mail-session.c				\
	mail-session.h				\
	mail-signature-editor.c			\
	mail-signature-editor.h			\
	mail-tools.c				\
	mail-tools.h				\
	mail-types.h				\
	mail-vfolder.c				\
	mail-vfolder.h				\
	message-list.c				\
	message-list.h				\
	message-tag-editor.c			\
	message-tag-editor.h			\
	message-tag-followup.c			\
	message-tag-followup.h

if ENABLE_SMIME
SMIME_LIB=$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_mail_la_LIBADD =					\
	$(top_builddir)/shell/importer/libevolution-importer.la	\
	$(top_builddir)/mail/importers/libevolution-mail-importers.la\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/widgets/misc/libefilterbar.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(SMIME_LIB)						\
	$(EVOLUTION_MAIL_LIBS)

libevolution_mail_la_LDFLAGS =				\
	-avoid-version -module

libevolution_mail_la_DEPENDENCIES = em-filter-i18n.h

# .server files

server_in_files = GNOME_Evolution_Mail.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

# Misc data to install
filterdir = $(privdatadir)
filter_DATA = filtertypes.xml vfoldertypes.xml searchtypes.xml

error_DATA = mail-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

em-filter-i18n.h: filtertypes.xml vfoldertypes.xml searchtypes.xml
	echo "/* Automatically generated. Do not edit. */" > $@;	      \
	cat $(srcdir)/filtertypes.xml $(srcdir)/vfoldertypes.xml $(srcdir)/searchtypes.xml |	      \
		sed -n -e 's:.*<title>\(.*\)</title>:char *s = N_("\1");:p' | \
		sort -u >> $@

glade_DATA = mail-config.glade mail-dialogs.glade
MARSHAL_GENERATED = em-marshal.c em-marshal.h
@EVO_MARSHAL_RULE@

etspec_DATA = message-list.etspec

EXTRA_DIST = 				\
	ChangeLog.pre-1-4		\
	em-marshal.list			\
	$(SPELL_IDL)			\
	$(error_DATA)			\
	$(error_i18n)			\
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
schema_in_files = evolution-mail.schemas.in.in
schema_DATA = $(schema_in_files:.schemas.in.in=-$(BASE_VERSION).schemas)
%-$(BASE_VERSION).schemas.in: %.schemas.in.in
	cp $< $@

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done \
	fi


# Prologue

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(SPELL_IDL_GENERATED) $(MARSHAL_GENERATED) $(server_DATA) $(error_i18n)

CLEANFILES = $(BUILT_SOURCES)

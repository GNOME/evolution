SUBDIRS = # importers FIXME

libexec_PROGRAMS =				\
	evolution-mbox-upgrade

importerdir = $(privlibdir)/evolution-mail-importers

INCLUDES =						\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/composer			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/shell/importer			\
	-I$(top_builddir)/shell/importer		\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(EVOLUTION_MAIL_CFLAGS)			\
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
	-DMAIL_IMPORTERSDIR=\""$(importerdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DG_LOG_DOMAIN=\"evolution-mail\"

component_LTLIBRARIES = libevolution-mail.la


# Code generation for Mailer.idl

MAILER_IDL = Mailer.idl

MAILER_IDL_GENERATED_H =		\
	Mailer.h
MAILER_IDL_GENERATED_C =		\
	Mailer-common.c				\
	Mailer-skels.c				\
	Mailer-stubs.c
MAILER_IDL_GENERATED = $(MAILER_IDL_GENERATED_C) $(MAILER_IDL_GENERATED_H)

$(MAILER_IDL_GENERATED_H): Mailer.idl 
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) $(srcdir)/Mailer.idl
$(MAILER_IDL_GENERATED_C): $(MAILER_IDL_GENERATED_H)


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
	$(MAILER_IDL_GENERATED)			\
	e-searching-tokenizer.c			\
	e-searching-tokenizer.h			\
	em-account-prefs.c			\
	em-account-prefs.h			\
	em-composer-prefs.c			\
	em-composer-prefs.h			\
	em-mailer-prefs.c			\
	em-mailer-prefs.h			\
	em-inline-filter.c			\
	em-inline-filter.h			\
	em-folder-selection.c			\
	em-folder-selection.h			\
	em-folder-selection-button.c		\
	em-folder-selection-button.h		\
	em-folder-selector.c			\
	em-folder-selector.h			\
	em-folder-view.c			\
	em-folder-view.h			\
	em-folder-browser.c			\
	em-folder-browser.h			\
	em-format.c				\
	em-format.h				\
	em-format-html.c			\
	em-format-html.h			\
	em-format-html-display.c		\
	em-format-html-display.h		\
	em-format-html-print.c			\
	em-format-html-print.h			\
	em-format-html-quote.c			\
	em-format-html-quote.h			\
	em-format-quote.c			\
	em-format-quote.h			\
	em-marshal.c				\
	em-marshal.h				\
	em-message-browser.c			\
	em-message-browser.h			\
	em-composer-utils.c			\
	em-composer-utils.h			\
	em-popup.c				\
	em-popup.h				\
	em-utils.c				\
	em-utils.h				\
	em-subscribe-editor.c			\
	em-subscribe-editor.h			\
	em-sync-stream.c			\
	em-sync-stream.h			\
	em-icon-stream.c			\
	em-icon-stream.h			\
	em-html-stream.c			\
	em-html-stream.h			\
	folder-browser-factory.c		\
	folder-browser-factory.h		\
	folder-info.c				\
	folder-info.h				\
	mail-account-editor.c			\
	mail-account-editor.h			\
	mail-account-gui.c			\
	mail-account-gui.h			\
	mail-autofilter.c			\
	mail-autofilter.h			\
	mail-component-factory.c		\
	mail-component.c			\
	mail-component.h			\
	mail-config.c				\
	mail-config.h				\
	mail-config-druid.c			\
	mail-config-druid.h			\
	mail-crypto.c				\
	mail-crypto.h				\
	mail-config-factory.c			\
	mail-config-factory.h			\
	mail-folder-cache.c			\
	mail-folder-cache.h			\
	mail-importer.c				\
	mail-importer.h				\
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
	message-tag-followup.h			\
	mail.h

# EPFIXME: Functionality to be brought back to life.
#
#	mail-local.c
#	mail-local.h

libevolution_mail_la_LIBADD =					\
	$(top_builddir)/shell/importer/libevolution-importer.la	\
	$(top_builddir)/camel/libcamel.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/widgets/misc/libefilterbar.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(EVOLUTION_MAIL_LIBS)

libevolution_mail_la_LDFLAGS =				\
	-avoid-version -module


# .server files

evolution_mbox_upgrade_SOURCES = evolution-mbox-upgrade.c
evolution_mbox_upgrade_LDADD = 				\
	$(top_builddir)/camel/libcamel.la 		\
	$(top_builddir)/e-util/libeutil.la 		\
	$(EVOLUTION_MAIL_LIBS)

server_in_files = GNOME_Evolution_Mail.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@COMPONENTDIR\@|$(componentdir)|" $< > $@
@INTLTOOL_SERVER_RULE@


# Misc data to install

glade_DATA = mail-config.glade local-config.glade subscribe-dialog.glade message-tags.glade mail-search.glade
MARSHAL_GENERATED = em-marshal.c em-marshal.h
@EVO_MARSHAL_RULE@

etspec_DATA = message-list.etspec

idl_DATA = $(MAILER_IDL)

EXTRA_DIST = 				\
	ChangeLog.pre-1-4		\
	em-marshal.list			\
	Mailer.idl 			\
	$(SPELL_IDL)			\
	$(glade_DATA)			\
	$(schema_DATA)			\
	$(server_in_files)		\
	$(server_DATA)			\
	$(etspec_DATA)


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
schema_DATA = evolution-mail.schemas

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done \
	fi


# Prologue

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(MAILER_IDL_GENERATED) $(SPELL_IDL_GENERATED) $(MARSHAL_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

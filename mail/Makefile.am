SUBDIRS = importers

libexec_PROGRAMS =				\
	evolution-mail
#	evolution-mail-upgrade

importerdir = $(privlibdir)/evolution-mail-importers

INCLUDES = \
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
	$(EVOLUTION_MAIL_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_IMAGES=\""$(datadir)/images/evolution"\"	\
	-DEVOLUTION_BUTTONSDIR=\""$(buttonsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(camel_providerdir)"\"	\
	-DMAIL_IMPORTERSDIR=\""$(importerdir)"\"	\
	-DG_DISABLE_DEPRECATED				\
	-DG_LOG_DOMAIN=\"evolution-mail\"

EVOLUTION_MAIL_CORBA_GENERATED = \
	Mailer.h		\
	Mailer-common.c		\
	Mailer-skels.c		\
	Mailer-stubs.c

SPELL_IDL = Spell.idl

IDL_GENERATED = 		\
	Spell.h			\
	Spell-common.c		\
	Spell-skels.c		\
	Spell-stubs.c

$(IDL_GENERATED): $(SPELL_IDL)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) $(srcdir)/Spell.idl

Spell-impl.o: Spell.h

evolution_mail_SOURCES =		\
	$(EVOLUTION_MAIL_CORBA_GENERATED)	\
	$(IDL_GENERATED)		\
	component-factory.c		\
	component-factory.h		\
	e-searching-tokenizer.c		\
	e-searching-tokenizer.h		\
	folder-browser.c		\
	folder-browser.h		\
	folder-browser-factory.c	\
	folder-browser-factory.h	\
	folder-browser-ui.c		\
	folder-browser-ui.h		\
	folder-info.c			\
	folder-info.h			\
	mail-account-editor.c		\
	mail-account-editor.h		\
	mail-account-gui.c		\
	mail-account-gui.h		\
	mail-accounts.c			\
	mail-accounts.h			\
	mail-autofilter.c		\
	mail-autofilter.h		\
	mail-callbacks.c		\
	mail-callbacks.h		\
	mail-composer-prefs.c		\
	mail-composer-prefs.h		\
	mail-config.c			\
	mail-config.h			\
	mail-config-druid.c		\
	mail-config-druid.h		\
	mail-crypto.c			\
	mail-crypto.h			\
	mail-config-factory.c		\
	mail-config-factory.h		\
	mail-preferences.c		\
	mail-preferences.h		\
	mail-display.c			\
	mail-display.h			\
	mail-display-stream.c		\
	mail-display-stream.h		\
	mail-folder-cache.c		\
	mail-folder-cache.h		\
	mail-format.c			\
	mail-identify.c			\
	mail-importer.c			\
	mail-importer.h			\
	mail-local.c			\
	mail-local.h			\
	mail-mt.c			\
	mail-mt.h			\
	mail-offline-handler.c		\
	mail-offline-handler.h		\
	mail-ops.c			\
	mail-ops.h			\
	mail-search.c			\
	mail-search.h			\
	mail-search-dialogue.c		\
	mail-search-dialogue.h		\
	mail-send-recv.c		\
	mail-send-recv.h		\
	mail-session.c			\
	mail-session.h			\
	mail-signature-editor.c		\
	mail-signature-editor.h		\
	mail-tools.c			\
	mail-tools.h			\
	mail-types.h			\
	mail-vfolder.c			\
	mail-vfolder.h			\
	message-browser.c		\
	message-browser.h		\
	main.c				\
	message-list.c			\
	message-list.h			\
	message-tag-editor.c		\
	message-tag-editor.h		\
	message-tag-followup.c		\
	message-tag-followup.h		\
	subscribe-dialog.c		\
	subscribe-dialog.h		\
	mail.h

# needs gtkhtml prop manager ported
#	mail-font-prefs.c		\
#	mail-font-prefs.h

evolution_mail_LDADD =						\
	$(top_builddir)/shell/importer/libevolution-importer.la	\
	$(top_builddir)/camel/libcamel.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/composer/libcomposer.a			\
	$(top_builddir)/widgets/misc/libemiscwidgets.a		\
	$(top_builddir)/widgets/misc/libefilterbar.a		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.a			\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(EVOLUTION_MAIL_LIBS)

evolution_mail_LDFLAGS =				\
	-export-dynamic

#evolution_mail_upgrade_SOURCES = upgrade-mailer.c
#evolution_mail_upgrade_LDADD = 				\
#	$(top_builddir)/camel/libcamel.la 		\
#	$(top_builddir)/e-util/libeutil.la 		\
#	$(EVOLUTION_MAIL_LIBS)

server_in_files = GNOME_Evolution_Mail.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

gladedir   = $(datadir)/evolution/glade
glade_DATA = mail-config.glade local-config.glade subscribe-dialog.glade message-tags.glade

etspecdir = $(datadir)/evolution/etspec/
etspec_DATA = mail-accounts.etspec message-list.etspec subscribe-dialog.etspec

iconsdir = $(datadir)/images/evolution
buttonsdir = $(datadir)/images/evolution/buttons

idldir = $(datadir)/idl
idl_DATA = Mailer.idl

$(EVOLUTION_MAIL_CORBA_GENERATED): Mailer.idl 
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) $(srcdir)/Mailer.idl

EXTRA_DIST = 				\
	Mailer.idl 			\
	$(SPELL_IDL)			\
	$(glade_DATA)			\
	$(server_in_files)		\
	$(server_DATA)			\
	$(etspec_DATA)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BUILT_SOURCES = $(EVOLUTION_MAIL_CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES) $(IDL_GENERATED)

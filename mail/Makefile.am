SUBDIRS = importers

bin_PROGRAMS =	evolution-mail

noinst_PROGRAMS = test-mail #test-thread

providerdir = $(libdir)/evolution/camel-providers/$(VERSION)
importerdir = $(libdir)/evolution/evolution-mail-importers/$(VERSION)

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
	-I$(top_builddir)/executive-summary		\
	-I$(top_srcdir)/executive-summary		\
	-I$(top_srcdir)/shell/importer			\
	-I$(top_builddir)/shell/importer		\
	$(MAILER_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"	\
	-DMAIL_IMPORTERSDIR=\""$(importerdir)"\"	\
	-DG_LOG_DOMAIN=\"evolution-mail\"

EVOLUTION_MAIL_CORBA_GENERATED = \
	Mail.h			\
	Mail-common.c		\
	Mail-skels.c		\
	Mail-stubs.c

evolution_mail_SOURCES =		\
	$(EVOLUTION_MAIL_CORBA_GENERATED)	\
	component-factory.c		\
	component-factory.h		\
	e-searching-tokenizer.c		\
	e-searching-tokenizer.h		\
	folder-browser.c		\
	folder-browser.h		\
	folder-browser-factory.c	\
	folder-browser-factory.h	\
	mail-accounts.c			\
	mail-accounts.h			\
	mail-account-editor.c		\
	mail-account-editor.h		\
	mail-account-gui.c		\
	mail-account-gui.h		\
	mail-autofilter.c		\
	mail-autofilter.h		\
	mail-callbacks.c		\
	mail-callbacks.h		\
	mail-config.c			\
	mail-config.h			\
	mail-config-druid.c		\
	mail-config-druid.h		\
	mail-crypto.c			\
	mail-crypto.h			\
	mail-display.c			\
	mail-display.h			\
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
	mail-summary.c			\
	mail-summary.h			\
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
	subscribe-dialog.c		\
	subscribe-dialog.h		\
	mail.h

evolution_mail_LDADD = 					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/composer/libcomposer.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a	\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libibex/libibex.la		\
	$(top_builddir)/filter/libfilter.la		\
	$(top_builddir)/executive-summary/evolution-services/libevolution-services.la	\
	$(top_builddir)/shell/importer/libevolution-importer.la				\
	$(top_builddir)/widgets/menus/libmenus.la	\
	$(MAILER_LIBS)

evolution_mail_LDFLAGS =				\
	-export-dynamic

#evolution_outlook_importer_SOURCES =			\
#	evolution-outlook-importer.c

#evolution_outlook_importer_LDADD =			\
#	$(BONOBO_GNOME_LIBS)				\
#	$(EXTRA_GNOME_LIBS)				\
#	$(top_builddir)/e-util/libeutil.la		\
#	$(top_builddir)/shell/importer/libevolution-importer.la	\
#	$(CAMEL_OBJS_EXTRA)				\
#	$(top_builddir)/camel/libcamel.la

test_mail_SOURCES = \
	test-mail.c

test_mail_LDADD = \
	$(BONOBO_HTML_GNOME_LIBS)

#test_thread_SOURCES =	\
#	mail-threads.c	\
#	mail-threads.h	\
#	test-thread.c
#
#test_thread_LDADD = 				\
#	$(top_builddir)/camel/libcamel.la	\
#	$(top_builddir)/e-util/libeutil.la	\
#	$(top_builddir)/libibex/libibex.la	\
#	$(BONOBO_HTML_GNOME_LIBS)		\
#	$(THREADS_LIBS)
#
#test_thread_CFLAGS = -g $(THREADS_CFLAGS)

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Mail.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@


gladedir   = $(datadir)/evolution/glade
glade_DATA = mail-config.glade local-config.glade

iconsdir = $(datadir)/images/evolution

$(EVOLUTION_MAIL_CORBA_GENERATED): Mail.idl 
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl -I `$(GNOME_CONFIG) --cflags idl` \
	-I `$(GNOME_CONFIG) --datadir`/idl $(srcdir)/Mail.idl

EXTRA_DIST = Mail.idl $(glade_DATA) $(oaf_in_files) $(oaf_DATA)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif

BUILT_SOURCES = $(EVOLUTION_MAIL_CORBA_GENERATED)
CLEANFILES   += $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

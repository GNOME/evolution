SUBDIRS = glade importer

INCLUDES =							\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libical			\
	-I$(top_srcdir)						\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"		\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"		\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(datadir)"\"				\
	-DG_LOG_DOMAIN=\"evolution-shell\"			\
	$(SHELL_CFLAGS)

noinst_PROGRAMS = evolution evolution-test-component

# Shell CORBA stuff

IDLS = \
	Evolution-Activity.idl			\
	Evolution-ConfigControl.idl		\
	Evolution-Offline.idl			\
	Evolution-Session.idl			\
	Evolution-Shell.idl			\
	Evolution-ShellComponent.idl		\
	Evolution-ShellComponentDnd.idl		\
	Evolution-ShellView.idl			\
	Evolution-Shortcuts.idl			\
	Evolution-Storage.idl			\
	Evolution-StorageSetView.idl		\
	Evolution-Wizard.idl			\
	Evolution-common.idl			\
	Evolution.idl

IDL_GENERATED_H = \
	Evolution.h

IDL_GENERATED_C = 		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

IDL_GENERATED = $(IDL_GENERATED_H) $(IDL_GENERATED_C)

$(IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(srcdir)/Evolution.idl

$(IDL_GENERATED_C): $(IDL_GENERATED_H)


# SelectNames CORBA stuff

SELECT_NAMES_IDL = \
	$(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

SELECT_NAMES_IDL_GENERATED_H =			\
	Evolution-Addressbook-SelectNames.h

SELECT_NAMES_IDL_GENERATED_C = 			   \
	Evolution-Addressbook-SelectNames-common.c \
	Evolution-Addressbook-SelectNames-skels.c  \
	Evolution-Addressbook-SelectNames-stubs.c

SELECT_NAMES_IDL_GENERATED = $(SELECT_NAMES_IDL_GENERATED_C) $(SELECT_NAMES_IDL_GENERATED_H)

$(SELECT_NAMES_IDL_GENERATED_H): $(SELECT_NAMES_IDL)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(SELECT_NAMES_IDL)

$(SELECT_NAMES_IDL_GENERATED_C): $(SELECT_NAMES_IDL_GENERATED_H)


# Wombat CORBA stuff

WOMBAT_IDL = \
	$(top_srcdir)/wombat/Evolution-Wombat.idl

WOMBAT_IDL_GENERATED_H =			\
	Evolution-Wombat.h

WOMBAT_IDL_GENERATED_C =			\
	Evolution-Wombat-common.c		\
	Evolution-Wombat-skels.c		\
	Evolution-Wombat-stubs.c

WOMBAT_IDL_GENERATED = $(WOMBAT_IDL_GENERATED_C) $(WOMBAT_IDL_GENERATED_H)

$(WOMBAT_IDL_GENERATED_H): $(WOMBAT_IDL)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(WOMBAT_IDL)

$(WOMBAT_IDL_GENERATED_C): $(WOMBAT_IDL_GENERATED_H)


# IDL install

idl_DATA = $(IDLS)

# Shell library

privlib_LTLIBRARIES =	\
	libeshell.la

eshellincludedir = $(privincludedir)/shell

eshellinclude_HEADERS = 			\
	Evolution.h				\
	e-folder.h				\
	e-folder-list.h				\
	e-folder-tree.h				\
	e-shell-corba-icon-utils.h		\
	evolution-activity-client.h		\
	evolution-config-control.h		\
	evolution-folder-selector-button.h	\
	evolution-session.h			\
	evolution-shell-client.h		\
	evolution-shell-component-client.h	\
	evolution-shell-component.h		\
	evolution-shell-component-dnd.h		\
	evolution-shell-component-utils.h	\
	evolution-shell-view.h			\
	evolution-storage-listener.h		\
	evolution-storage-set-view-listener.h	\
	evolution-storage.h			\
	evolution-wizard.h

libeshell_la_SOURCES = 				\
	$(IDL_GENERATED)			\
	e-folder.c				\
	e-folder-list.c				\
	e-folder-tree.c				\
	e-shell-corba-icon-utils.c		\
	evolution-activity-client.c		\
	evolution-config-control.c		\
	evolution-folder-selector-button.c	\
	evolution-session.c			\
	evolution-shell-client.c		\
	evolution-shell-component-client.c	\
	evolution-shell-component.c		\
	evolution-shell-component-dnd.c		\
	evolution-shell-component-utils.c	\
	evolution-shell-view.c			\
	evolution-storage-listener.c		\
	evolution-storage-set-view-listener.c	\
	evolution-storage.c			\
	evolution-wizard.c			\
	e-shell-marshal.c			\
	$(eshellinclude_HEADERS)

libeshell_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la

# Evolution executable

evolution_SOURCES =				\
	$(SELECT_NAMES_IDL_GENERATED)		\
	$(WOMBAT_IDL_GENERATED)			\
	e-activity-handler.c			\
	e-activity-handler.h			\
	e-component-info.c			\
	e-component-info.h			\
	e-component-registry.c			\
	e-component-registry.h			\
	e-config-upgrade.c			\
	e-config-upgrade.h			\
	e-corba-config-page.c			\
	e-corba-config-page.h			\
	e-corba-shortcuts.c			\
	e-corba-shortcuts.h			\
	e-corba-storage-registry.c		\
	e-corba-storage-registry.h		\
	e-corba-storage.c			\
	e-corba-storage.h			\
	e-folder-dnd-bridge.c			\
	e-folder-dnd-bridge.h			\
	e-folder-type-registry.c		\
	e-folder-type-registry.h		\
	e-history.c				\
	e-history.h				\
	e-icon-factory.c			\
	e-icon-factory.h			\
	e-local-folder.c			\
	e-local-folder.h			\
	e-local-storage.c			\
	e-local-storage.h			\
	e-setup.c				\
	e-setup.h				\
	e-shell-about-box.c			\
	e-shell-about-box.h			\
	e-shell-config.c			\
	e-shell-config.h			\
	e-shell-config-autocompletion.c		\
	e-shell-config-autocompletion.h		\
	e-shell-config-offline.c		\
	e-shell-config-offline.h		\
	e-shell-config-default-folders.c	\
	e-shell-config-default-folders.h	\
	e-shell-config-folder-settings.c	\
	e-shell-config-folder-settings.h	\
	e-shell-constants.h			\
	e-shell-folder-commands.c		\
	e-shell-folder-commands.h		\
	e-shell-folder-creation-dialog.c	\
	e-shell-folder-creation-dialog.h	\
	e-shell-folder-selection-dialog.c	\
	e-shell-folder-selection-dialog.h	\
	e-shell-folder-title-bar.c		\
	e-shell-folder-title-bar.h		\
	e-shell-importer.c			\
	e-shell-importer.h			\
	e-shell-offline-handler.c		\
	e-shell-offline-handler.h		\
	e-shell-offline-sync.c			\
	e-shell-offline-sync.h			\
	e-shell-settings-dialog.c		\
	e-shell-settings-dialog.h		\
	e-shell-shared-folder-picker-dialog.c 	\
	e-shell-shared-folder-picker-dialog.h 	\
	e-shell-startup-wizard.c		\
	e-shell-startup-wizard.h		\
	e-shell-user-creatable-items-handler.c	\
	e-shell-user-creatable-items-handler.h	\
	e-shell-utils.c				\
	e-shell-utils.h				\
	e-shell-view-menu.c			\
	e-shell-view-menu.h			\
	e-shell-view.c				\
	e-shell-view.h				\
	e-shell.c				\
	e-shell.h				\
	e-shortcuts-view-model.c		\
	e-shortcuts-view-model.h		\
	e-shortcuts-view.c			\
	e-shortcuts-view.h			\
	e-shortcuts.c				\
	e-shortcuts.h				\
	e-splash.c				\
	e-splash.h				\
	e-storage-set-view.c			\
	e-storage-set-view.h			\
	e-storage-set.c				\
	e-storage-set.h				\
	e-storage.c				\
	e-storage.h				\
	e-task-bar.c				\
	e-task-bar.h				\
	e-task-widget.c				\
	e-task-widget.h				\
	e-uri-schema-registry.c			\
	e-uri-schema-registry.h			\
	evolution-storage-set-view.c		\
	evolution-storage-set-view.h		\
	evolution-storage-set-view-factory.c	\
	evolution-storage-set-view-factory.h	\
	main.c

evolution_LDADD =							\
	libeshell.la							\
	importer/libevolution-importer.la				\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libical/src/libical/libical-evolution.la	\
	$(SHELL_LIBS)

# Test component

evolution_test_component_SOURCES =	\
	evolution-test-component.c

evolution_test_component_LDADD = 	\
	libeshell.la			\
	$(SHELL_LIBS)

install-test-component: evolution-test-component
	$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) evolution-test-component $(DESTDIR)$(bindir)/evolution-test-component
	$(mkinstalldirs) $(DESTDIR)$(serverdir)
	$(INSTALL_DATA) $(srcdir)/GNOME_Evolution_TestComponent.server $(DESTDIR)$(serverdir)/GNOME_Evolution_TestComponent.server

# Misc stuff

server_in_files = GNOME_Evolution_Shell.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@BINDIR\@|$(bindir)|"				\
	    -e "s|\@VERSION\@|$(BASE_VERSION)|"				\
	    $< > $@

etspec_DATA = e-storage-set-view.etspec

@INTLTOOL_SERVER_RULE@

icons = 			\
	check-empty.xpm		\
	check-filled.xpm	\
	check-missing.xpm

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = apps_evolution_shell.schemas

install-data-local:
	if test -z "$(DESTDIR)" ; then												\
		for p in $(schema_DATA) ; do											\
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p;	\
		done														\
	fi

install-evolution:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) evolution $(DESTDIR)$(bindir)/evolution-$(BASE_VERSION)
if DEFAULT_BINARY
	cd $(DESTDIR)$(bindir) && rm -f evolution && $(LN_S) evolution-$(BASE_VERSION) evolution
endif

if HAVE_DTAPPINTEGRATE

install-exec-local: install-evolution
	$(mkinstalldirs) $(DESTDIR)$(libexecdir)
	mv $(DESTDIR)$(bindir)/evolution-$(BASE_VERSION) $(DESTDIR)$(libexecdir)/evolution-$(BASE_VERSION)
	$(INSTALL_PROGRAM) evolution-nognome $(DESTDIR)$(bindir)/evolution-$(BASE_VERSION) 

else

install-exec-local: install-evolution

endif


# GLib marshalling cruft

e-shell-marshal.h: e-shell-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=e_shell_marshal e-shell-marshal.list --header > e-shell-marshal.h.tmp \
	&& mv e-shell-marshal.h.tmp e-shell-marshal.h ) \
	|| ( rm -f e-shell-marshal.h.tmp && exit 1 )

e-shell-marshal.c: e-shell-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=e_shell_marshal e-shell-marshal.list --body > e-shell-marshal.c.tmp \
	&& mv e-shell-marshal.c.tmp e-shell-marshal.c ) \
	|| ( rm -f e-shell-marshal.c.tmp && exit 1 )

MARSHAL_GENERATED = e-shell-marshal.c e-shell-marshal.h

# Extra dist stuff

EXTRA_DIST = 					\
	$(IDLS)					\
	$(server_in_files)			\
	$(server_DATA)				\
	$(etspec_DATA)				\
	$(schema_DATA)				\
	$(icons)				\
	ChangeLog.pre-1-4			\
	GNOME_Evolution_TestComponent.server	\
	e-shell-marshal.list			\
	evolution-nognome.in

# Purify support

if ENABLE_PURIFY

PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution.pure

evolution.pure: evolution
	@rm -f evolution.pure
	$(PLINK) $(evolution_LDFLAGS) $(evolution_OBJECTS) $(evolution_LDADD) $(LIBS)

endif

BUILT_SOURCES = $(IDL_GENERATED) $(SELECT_NAMES_IDL_GENERATED) $(MARSHAL_GENERATED) $(server_DATA) $(WOMBAT_IDL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

noinst_SCRIPTS = evolution-nognome


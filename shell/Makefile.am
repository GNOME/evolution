SUBDIRS = glade importer

INCLUDES = -O							\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libical			\
	-I$(top_srcdir)						\
	-DEVOLUTION_IMAGES=\""$(datadir)/images/evolution"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"   		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            	\
	-DEVOLUTION_GLADEDIR=\"$(datadir)/evolution/glade\"	\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"		\
	-DPREFIX=\"$(prefix)\"					\
	-DG_LOG_DOMAIN=\"evolution-shell\"			\
	$(SHELL_CFLAGS)

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

IDL_GENERATED = \
	Evolution.h		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

Evolution-impl.o: Evolution.h

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		$(srcdir)/Evolution.idl

# SelectNames CORBA stuff

SELECT_NAMES_IDL = \
	$(top_srcdir)/addressbook/gui/component/select-names/Evolution-Addressbook-SelectNames.idl

SELECT_NAMES_IDL_GENERATED = 			   \
	Evolution-Addressbook-SelectNames.h	   \
	Evolution-Addressbook-SelectNames-common.c \
	Evolution-Addressbook-SelectNames-skels.c  \
	Evolution-Addressbook-SelectNames-stubs.c

$(SELECT_NAMES_IDL_GENERATED): $(SELECT_NAMES_IDL)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		$(SELECT_NAMES_IDL)

# Wombat CORBA stuff

WOMBAT_IDL = $(top_srcdir)/wombat/Evolution-Wombat.idl

WOMBAT_IDL_GENERATED =				\
	Evolution-Wombat.h			\
	Evolution-Wombat-common.c		\
	Evolution-Wombat-skels.c		\
	Evolution-Wombat-stubs.c

$(WOMBAT_IDL_GENERATED): $(WOMBAT_IDL)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` $(WOMBAT_IDL)

# IDL install

idldir = $(datadir)/idl

idl_DATA = $(IDLS)

# Shell library

lib_LTLIBRARIES =	\
	libeshell.la

eshellincludedir = $(includedir)/evolution/shell

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
	$(eshellinclude_HEADERS)

# Evolution executable

bin_PROGRAMS = evolution

evolution_SOURCES =				\
	$(SELECT_NAMES_IDL_GENERATED)		\
	$(WOMBAT_IDL_GENERATED)			\
	e-activity-handler.c			\
	e-activity-handler.h			\
	e-component-info.c			\
	e-component-info.h			\
	e-component-registry.c			\
	e-component-registry.h			\
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
	e-gray-bar.c				\
	e-gray-bar.h				\
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
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.a	\
	$(top_builddir)/widgets/misc/libemiscwidgets.a			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libical/src/libical/libical-evolution.la	\
	$(SHELL_LIBS)

# Test component

noinst_PROGRAMS =			\
	evolution-test-component

evolution_test_component_SOURCES =	\
	evolution-test-component.c

evolution_test_component_LDADD = 	\
	libeshell.la			\
	$(SHELL_LIBS)

install-test-component: evolution-test-component
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) evolution-test-component $(DESTDIR)$(bindir)/evolution-test-component
	$(mkinstalldirs) $(DESTDIR)$(oafdir)
	$(INSTALL_DATA) $(srcdir)/GNOME_Evolution_TestComponent.oaf $(DESTDIR)$(oafdir)/GNOME_Evolution_TestComponent.oaf

# Misc stuff

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Shell.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

etspecdir = $(datadir)/evolution/etspec/
etspec_DATA = \
	e-storage-set-view.etspec

@XML_I18N_MERGE_OAF_RULE@

icons = 			\
	check-empty.xpm		\
	check-filled.xpm	\
	check-missing.xpm

EXTRA_DIST = 					\
	$(IDLS)					\
	$(oaf_in_files)				\
	$(oaf_DATA)				\
	$(etspec_DATA)				\
	$(icons)				\
	GNOME_Evolution_TestComponent.oaf	\
	evolution-nognome.in

# Purify support

if ENABLE_PURIFY

PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution.pure

evolution.pure: evolution
	@rm -f evolution.pure
	$(PLINK) $(evolution_LDFLAGS) $(evolution_OBJECTS) $(evolution_LDADD) $(LIBS)

endif

CLEANFILES = $(IDL_GENERATED) $(SELECT_NAMES_IDL_GENERATED)
BUILT_SOURCES = $(IDL_GENERATED) $(SELECT_NAMES_IDL_GENERATED)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)


if HAVE_DTAPPINTEGRATE

bin_SCRIPTS = evolution-nognome

endif
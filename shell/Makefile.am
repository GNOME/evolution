SUBDIRS = glade importer

INCLUDES =							\
	-DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED		\
	-DBONOBO_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED	\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/misc				\
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
	$(TZDIALOG_CFLAGS)					\
	$(SHELL_CFLAGS)

noinst_PROGRAMS = evolution evolution-test-component

# Shell CORBA stuff

IDLS = \
	Evolution-Activity.idl			\
	Evolution-ConfigControl.idl		\
	Evolution-Component.idl			\
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


# Data Server CORBA stuff
DATASERVER_IDL_GENERATED_H =			\
	Evolution-DataServer.h

DATASERVER_IDL_GENERATED_C =			\
	Evolution-DataServer-common.c		\
	Evolution-DataServer-skels.c		\
	Evolution-DataServer-stubs.c

DATASERVER_IDL_GENERATED = $(DATASERVER_IDL_GENERATED_C) $(DATASERVER_IDL_GENERATED_H)

$(DATASERVER_IDL_GENERATED_H): $(DATASERVER_IDL)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(DATASERVER_IDL)

$(DATASERVER_IDL_GENERATED_C): $(DATASERVER_IDL_GENERATED_H)


# IDL install

idl_DATA = $(IDLS)

# Shell library

privlib_LTLIBRARIES =	\
	libeshell.la

eshellincludedir = $(privincludedir)/shell

eshellinclude_HEADERS = 			\
	Evolution.h				\
	e-corba-storage.h			\
	e-folder-dnd-bridge.h			\
	e-folder-list.h				\
	e-folder-selection-dialog.h		\
	e-folder-tree.h				\
	e-folder-type-registry.h		\
	e-folder.h				\
	e-icon-factory.h			\
	e-shell-corba-icon-utils.h		\
	e-shell-utils.h				\
	e-storage-browser.h			\
	e-storage-set-view.h			\
	e-storage-set.h				\
	e-storage.h				\
	evolution-activity-client.h		\
	evolution-config-control.h		\
	evolution-folder-selector-button.h	\
	evolution-session.h			\
	evolution-shell-client.h		\
	evolution-shell-component-client.h	\
	evolution-shell-component-dnd.h		\
	evolution-shell-component-utils.h	\
	evolution-shell-component.h		\
	evolution-shell-view.h			\
	evolution-storage-listener.h		\
	evolution-storage-set-view-listener.h	\
	evolution-storage.h			\
	evolution-wizard.h

libeshell_la_SOURCES = 				\
	$(IDL_GENERATED)			\
	$(MARSHAL_GENERATED)			\
	e-corba-storage.c			\
	e-folder-dnd-bridge.c			\
	e-folder-list.c				\
	e-folder-selection-dialog.c		\
	e-folder-tree.c				\
	e-folder-type-registry.c		\
	e-folder.c				\
	e-icon-factory.c			\
	e-shell-corba-icon-utils.c		\
	e-shell-utils.c				\
	e-storage-browser.c			\
	e-storage-set-view.c			\
	e-storage-set.c				\
	e-storage.c				\
	evolution-activity-client.c		\
	evolution-config-control.c		\
	evolution-folder-selector-button.c	\
	evolution-session.c			\
	evolution-shell-client.c		\
	evolution-shell-component-client.c	\
	evolution-shell-component-dnd.c		\
	evolution-shell-component-utils.c	\
	evolution-shell-component.c		\
	evolution-shell-view.c			\
	evolution-storage-listener.c		\
	evolution-storage-set-view-listener.c	\
	evolution-storage.c			\
	evolution-wizard.c			\
	$(eshellinclude_HEADERS)

libeshell_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la

# Evolution executable

evolution_SOURCES =				\
	$(SELECT_NAMES_IDL_GENERATED)		\
	$(DATASERVER_IDL_GENERATED)		\
	e-component-registry.c			\
	e-component-registry.h			\
	e-config-upgrade.c			\
	e-config-upgrade.h			\
	e-corba-config-page.c			\
	e-corba-config-page.h			\
	e-corba-storage-registry.c		\
	e-corba-storage-registry.h		\
	e-history.c				\
	e-history.h				\
	e-local-folder.c			\
	e-local-folder.h			\
	e-local-storage.c			\
	e-local-storage.h			\
	e-setup.c				\
	e-setup.h				\
	e-shell-about-box.c			\
	e-shell-about-box.h			\
	e-shell-constants.h			\
	e-shell-folder-title-bar.c		\
	e-shell-folder-title-bar.h		\
	e-shell-settings-dialog.c		\
	e-shell-settings-dialog.h		\
	e-shell-startup-wizard.c		\
	e-shell-startup-wizard.h		\
	e-shell-view.h				\
	e-shell-window-commands.c		\
	e-shell-window-commands.h		\
	e-shell-window.c			\
	e-shell-window.h			\
	e-shell.c				\
	e-shell.h				\
	e-sidebar.c				\
	e-sidebar.h				\
	e-user-creatable-items-handler.c	\
	e-user-creatable-items-handler.h	\
	e-task-bar.c				\
	e-task-bar.h				\
	e-task-widget.c				\
	e-task-widget.h				\
	e-uri-schema-registry.c			\
	e-uri-schema-registry.h			\
	main.c

evolution_LDADD =							\
	libeshell.la							\
	importer/libevolution-importer.la				\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(TZDIALOG_LIBS)						\
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
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

etspec_DATA = e-storage-set-view.etspec

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


MARSHAL_GENERATED = e-shell-marshal.c e-shell-marshal.h
@EVO_MARSHAL_RULE@

# Extra dist stuff

EXTRA_DIST = 					\
	$(IDLS)					\
	$(server_in_files)			\
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

BUILT_SOURCES = $(IDL_GENERATED) $(SELECT_NAMES_IDL_GENERATED) $(MARSHAL_GENERATED) $(server_DATA) $(DATASERVER_IDL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

noinst_SCRIPTS = evolution-nognome


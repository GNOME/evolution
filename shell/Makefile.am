SUBDIRS = importer

INCLUDES =							\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)						\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"		\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"		\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(datadir)"\"				\
	-DG_LOG_DOMAIN=\"evolution-shell\"			\
	$(TZDIALOG_CFLAGS)					\
	$(SHELL_CFLAGS)						\
	$(EVOLUTION_TEST_CFLAGS)

noinst_PROGRAMS = evolution

# Shell CORBA stuff

IDLS = \
	Evolution-ConfigControl.idl		\
	Evolution-Component.idl			\
	Evolution-Offline.idl			\
	Evolution-Shell.idl			\
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
	e-shell-corba-icon-utils.h		\
	e-shell-utils.h				\
	e-user-creatable-items-handler.h	\
	evolution-config-control.h		\
	evolution-shell-component-utils.h	\
	evolution-wizard.h

libeshell_la_SOURCES = 				\
	$(IDL_GENERATED)			\
	$(MARSHAL_GENERATED)			\
	e-shell-corba-icon-utils.c		\
	e-shell-utils.c				\
	e-user-creatable-items-handler.c	\
	evolution-config-control.c		\
	evolution-shell-component-utils.c	\
	evolution-wizard.c			\
	$(eshellinclude_HEADERS)

libeshell_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la

# Evolution executable

evolution_SOURCES =				\
	$(DATASERVER_IDL_GENERATED)		\
	e-component-registry.c			\
	e-component-registry.h			\
	e-config-upgrade.c			\
	e-config-upgrade.h			\
	e-corba-config-page.c			\
	e-corba-config-page.h			\
	e-history.c				\
	e-history.h				\
	e-shell-constants.h			\
	e-shell-folder-title-bar.c		\
	e-shell-folder-title-bar.h		\
	e-shell-importer.c			\
	e-shell-importer.h			\
	e-shell-offline-handler.c		\
	e-shell-offline-handler.h		\
	e-shell-settings-dialog.c		\
	e-shell-settings-dialog.h		\
	e-shell-startup-wizard.c		\
	e-shell-startup-wizard.h		\
	e-shell-window-commands.c		\
	e-shell-window-commands.h		\
	e-shell-window.c			\
	e-shell-window.h			\
	e-shell.c				\
	e-shell.h				\
	e-sidebar.c				\
	e-sidebar.h				\
	es-event.c				\
	es-event.h				\
	es-menu.c				\
	es-menu.h				\
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

if ENABLE_TEST_COMPONENT
component_LTLIBRARIES = libevolution-test.la
endif

libevolution_test_la_SOURCES =		\
	$(IDL_GENERATED)		\
	evolution-test-component.c	\
	evolution-test-component.h

libevolution_test_la_LIBADD = 		\
	libeshell.la			\
	$(EVOLUTION_TEST_LIBS)

libevolution_test_la_LDFLAGS = -avoid-version -module

if ENABLE_TEST_COMPONENT
testserver_in_files = GNOME_Evolution_Test.server.in.in
testserver_DATA = $(testserver_in_files:.server.in.in=_$(BASE_VERSION).server)
testserverdir = $(serverdir)
endif

# Misc stuff

server_in_files = GNOME_Evolution_Shell.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

error_DATA = shell-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

glade_DATA =						\
	e-active-connection-dialog.glade		\
	evolution-startup-wizard.glade

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_shell.schemas.in.in
schema_DATA = $(schema_in_files:.schemas.in.in=-$(BASE_VERSION).schemas)
%-$(BASE_VERSION).schemas.in: %.schemas.in.in
	cp $< $@

@INTLTOOL_SCHEMAS_RULE@

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
	GNOME_Evolution_Test.server.in.in	\
	$(error_DATA)				\
	$(error_i18n)				\
	$(glade_DATA)				\
	$(schema_in_files)			\
	ChangeLog.pre-1-4			\
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

BUILT_SOURCES = $(IDL_GENERATED) $(MARSHAL_GENERATED) $(server_DATA) $(testserver_DATA) $(DATASERVER_IDL_GENERATED) $(error_i18n)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

noinst_SCRIPTS = evolution-nognome


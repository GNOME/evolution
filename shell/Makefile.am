if ENABLE_TEST_COMPONENT
SUBDIRS = . test
endif

AM_CPPFLAGS =							\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell                                 \
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"		\
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
	$(SHELL_CFLAGS)

if NM_SUPPORT
AM_CPPFLAGS += \
	$(DBUS_GLIB_CFLAGS) \
	$(NM_FLAGS)
endif

bin_PROGRAMS = evolution

# Shell CORBA stuff

IDLS = \
	Evolution-ConfigControl.idl		\
	Evolution-Component.idl			\
	Evolution-Shell.idl			\
	Evolution.idl

IDL_GENERATED_H = \
	Evolution.h

IDL_GENERATED_C = 		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

idl_defines = \
	-D__evolution_shell_COMPILATION

IDL_GENERATED = $(IDL_GENERATED_H) $(IDL_GENERATED_C)

$(IDL_GENERATED_H): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(idl_defines) $(IDL_INCLUDES) $(srcdir)/Evolution.idl

$(IDL_GENERATED_C): $(IDL_GENERATED_H)

if NM_SUPPORT
NM_SUPPORT_FILES = e-shell-nm.c 
endif

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

privsolib_LTLIBRARIES =	\
	libeshell.la

eshellincludedir = $(privincludedir)/shell

eshellinclude_HEADERS = 			\
	Evolution.h				\
	e-component-view.h			\
	e-user-creatable-items-handler.h	\
	evolution-config-control.h		\
	evolution-component.h			\
	evolution-listener.h			\
	evolution-shell-component-utils.h	\
	es-event.h				\
	es-menu.h

libeshell_la_SOURCES =				\
	$(IDL_GENERATED)			\
	e-component-view.c			\
	evolution-component.c			\
	evolution-listener.c			\
	e-user-creatable-items-handler.c	\
	evolution-config-control.c		\
	evolution-shell-component-utils.c	\
	$(eshellinclude_HEADERS)

libeshell_la_LDFLAGS = $(NO_UNDEFINED)

libeshell_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(SHELL_LIBS)

# Evolution executable

if HAVE_WINDRES
EVOLUTIONICONRC = evolution-icon.o
endif

evolution_SOURCES =				\
	$(DATASERVER_IDL_GENERATED)		\
	$(NM_SUPPORT_FILES)			\
	e-component-registry.c			\
	e-component-registry.h			\
	e-config-upgrade.c			\
	e-config-upgrade.h			\
	e-corba-config-page.c			\
	e-corba-config-page.h			\
	e-shell.c				\
	e-shell.h				\
	e-shell-constants.h			\
	e-shell-importer.c			\
	e-shell-importer.h			\
	e-shell-settings-dialog.c		\
	e-shell-settings-dialog.h		\
	e-shell-window-commands.c		\
	e-shell-window-commands.h		\
	e-shell-window.c			\
	e-shell-window.h			\
	e-shell-view.c				\
	e-shell-view.h				\
	e-sidebar.c				\
	e-sidebar.h				\
	es-event.c				\
	es-menu.c				\
	main.c					

evolution_LDADD =							\
	libeshell.la							\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(TZDIALOG_LIBS)						\
	$(SHELL_LIBS) \
	$(EVOLUTIONICONRC)

if OS_WIN32
evolution_LDFLAGS = -mwindows
endif

# Misc stuff

server_in_files = GNOME_Evolution_Shell.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

error_DATA = shell.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

glade_DATA =						\
	e-active-connection-dialog.glade		\
	import.glade

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_shell.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

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

# Extra dist stuff

EXTRA_DIST = 					\
	$(IDLS)					\
	$(server_in_files)			\
	shell.error.xml				\
	$(glade_DATA)				\
	$(schema_in_files)			\
	ChangeLog.pre-1-4			\
	evolution-nognome.in			\
	evolution-icon.rc			\
	evolution.ico				\
	evolution-mail.ico			\
	evolution-memos.ico			\
	evolution-tasks.ico

# Purify support

if ENABLE_PURIFY

PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution.pure

evolution.pure: evolution
	@rm -f evolution.pure
	$(PLINK) $(evolution_LDFLAGS) $(evolution_OBJECTS) $(evolution_LDADD) $(LIBS)

endif

BUILT_SOURCES = $(IDL_GENERATED) $(server_DATA) $(DATASERVER_IDL_GENERATED) $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

noinst_SCRIPTS = evolution-nognome

evolution-icon.o: evolution.ico evolution-mail.ico evolution-memos.ico evolution-tasks.ico
	$(WINDRES) evolution-icon.rc evolution-icon.o

-include $(top_srcdir)/git.mk

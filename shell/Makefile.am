if ENABLE_TEST_COMPONENT
SUBDIRS = . test
endif

INCLUDES =							\
	-I$(top_srcdir)/e-util					\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/menus				\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell                                 \
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"		\
	-DEVOLUTION_MODULEDIR=\""$(moduledir)"\"		\
	-DEVOLUTION_RULEDIR=\""$(ruledir)"\"			\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"		\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(datadir)"\"				\
	-DG_LOG_DOMAIN=\"evolution-shell\"			\
	$(TZDIALOG_CFLAGS)					\
	$(SHELL_CFLAGS)

noinst_PROGRAMS = evolution

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


# Shell library

privsolib_LTLIBRARIES =	\
	libeshell.la

eshellincludedir = $(privincludedir)/shell

eshellinclude_HEADERS = 			\
	e-activity-handler.h			\
	e-shell-common.h			\
	e-shell-content.h			\
	e-shell-module.h			\
	e-shell-sidebar.h			\
	e-shell-switcher.h			\
	e-shell-taskbar.h			\
	e-shell-view.h				\
	e-shell-window.h			\
	e-shell-window-actions.h		\
	es-event.h

libeshell_la_SOURCES =				\
	$(IDL_GENERATED)			\
	$(MARSHAL_GENERATED)			\
	e-activity-handler.c			\
	e-shell-content.c			\
	e-shell-module.c			\
	e-shell-sidebar.c			\
	e-shell-switcher.c			\
	e-shell-taskbar.c			\
	e-shell-view.c				\
	e-shell-window.c			\
	e-shell-window-actions.c		\
	$(eshellinclude_HEADERS)

libeshell_la_LDFLAGS = $(NO_UNDEFINED)

libeshell_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(SHELL_LIBS)

# Evolution executable

evolution_SOURCES =				\
	$(DATASERVER_IDL_GENERATED)		\
	$(NM_SUPPORT_FILES)			\
	e-config-upgrade.c			\
	e-config-upgrade.h			\
	e-shell.c				\
	e-shell.h				\
	e-shell-constants.h			\
	e-shell-importer.c			\
	e-shell-importer.h			\
	e-shell-registry.c			\
	e-shell-registry.h			\
	e-shell-window-private.c		\
	e-shell-window-private.h		\
	es-event.c				\
	main.c					

evolution_LDADD =							\
	libeshell.la							\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(TZDIALOG_LIBS)						\
	$(SHELL_LIBS)

# Misc stuff

error_DATA = shell.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

glade_DATA =						\
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

install-evolution: $(install-privsolibLTLIBRARIES)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) evolution$(EXEEXT) $(DESTDIR)$(bindir)/evolution$(EXEEXT)

uninstall-evolution:
	$(LIBTOOL) --mode=uninstall rm -rf $(DESTDIR)$(bindir)/evolution$(EXEEXT)

if HAVE_DTAPPINTEGRATE

install-exec-local: install-evolution
	$(mkinstalldirs) $(DESTDIR)$(libexecdir)
	mv $(DESTDIR)$(bindir)/evolution-$(BASE_VERSION) $(DESTDIR)$(libexecdir)/evolution-$(BASE_VERSION)
	$(INSTALL_PROGRAM) evolution-nognome $(DESTDIR)$(bindir)/evolution-$(BASE_VERSION) 

uninstall-local: uninstall-evolution
	rm -rf $(DESTDIR)$(libexecdir)/evolution-$(BASE_VERSION)
	rm -rf $(DESTDIR)$(bindir)/evolution-$(BASE_VERSION)/evolution-nognome
else

install-exec-local: install-evolution
uninstall-local: uninstall-evolution

endif

MARSHAL_GENERATED =				\
	e-shell-marshal.c			\
	e-shell-marshal.h
@EVO_MARSHAL_RULE@

# Extra dist stuff

EXTRA_DIST = 					\
	shell.error.xml				\
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

BUILT_SOURCES = $(MARSHAL_GENERATED) $(server_DATA) $(DATASERVER_IDL_GENERATED) $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

noinst_SCRIPTS = evolution-nognome


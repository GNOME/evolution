if ENABLE_TEST_COMPONENT
SUBDIRS = . test
endif

if NM_SUPPORT
NM_CPPFLAGS = \
	$(DBUS_GLIB_CFLAGS) \
	$(NM_CFLAGS)
NM_SUPPORT_FILES = e-shell-nm.c
endif

# Executable
bin_PROGRAMS = evolution

# Shell library
privsolib_LTLIBRARIES =	libeshell.la

eshellincludedir = $(privincludedir)/shell

eshellinclude_HEADERS = 			\
	e-shell.h				\
	e-shell-backend.h			\
	e-shell-common.h			\
	e-shell-content.h			\
	e-shell-settings.h			\
	e-shell-sidebar.h			\
	e-shell-switcher.h			\
	e-shell-taskbar.h			\
	e-shell-utils.h				\
	e-shell-view.h				\
	e-shell-window.h			\
	e-shell-window-actions.h		\
	es-event.h

libeshell_la_CPPFLAGS =						\
	$(AM_CPPFLAGS)						\
	$(NM_CPPFLAGS)						\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/menus				\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-DEVOLUTION_BINDIR=\""$(bindir)"\"			\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"		\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"		\
	-DEVOLUTION_MODULEDIR=\""$(moduledir)"\"		\
	-DEVOLUTION_RULEDIR=\""$(privdatadir)"\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(datadir)"\"				\
	-DG_LOG_DOMAIN=\"evolution-shell\"			\
	$(TZDIALOG_CFLAGS)					\
	$(SHELL_CFLAGS)

libeshell_la_SOURCES =				\
	$(NM_SUPPORT_FILES)			\
	$(eshellinclude_HEADERS)		\
	e-shell.c				\
	e-shell-backend.c			\
	e-shell-content.c			\
	e-shell-settings.c			\
	e-shell-sidebar.c			\
	e-shell-switcher.c			\
	e-shell-taskbar.c			\
	e-shell-utils.c				\
	e-shell-view.c				\
	e-shell-window.c			\
	e-shell-window-private.c		\
	e-shell-window-private.h		\
	e-shell-migrate.c			\
	e-shell-migrate.h			\
	e-shell-window-actions.c		\
	es-event.c

libeshell_la_LDFLAGS = $(NO_UNDEFINED)

libeshell_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/filter/libfilter.la		\
	$(top_builddir)/smclient/libeggsmclient.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(top_builddir)/widgets/menus/libmenus.la	\
	$(SHELL_LIBS)

if NM_SUPPORT
libeshell_la_LIBADD += $(DBUS_GLIB_LIBS)
endif

# Evolution executable

if HAVE_WINDRES
EVOLUTIONICONRC = evolution-icon.o
endif

evolution_CPPFLAGS =						\
	$(AM_CPPFLAGS)						\
	$(NM_CPPFLAGS)						\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/widgets/menus				\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-DEVOLUTION_BINDIR=\""$(bindir)"\"			\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"			\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"		\
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\"		\
	-DEVOLUTION_ICONDIR=\""$(icondir)"\"			\
	-DEVOLUTION_MODULEDIR=\""$(moduledir)"\"		\
	-DEVOLUTION_RULEDIR=\""$(privdatadir)"\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(datadir)"\"				\
	-DG_LOG_DOMAIN=\"evolution-shell\"			\
	$(TZDIALOG_CFLAGS)					\
	$(SHELL_CFLAGS)

evolution_SOURCES =				\
	e-config-upgrade.c			\
	e-config-upgrade.h			\
	main.c					

evolution_LDADD =							\
	libeshell.la							\
	$(top_builddir)/widgets/e-timezone-dialog/libetimezonedialog.la	\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la				\
	$(TZDIALOG_LIBS)						\
	$(SHELL_LIBS) \
	$(EVOLUTIONICONRC)

if OS_WIN32
evolution_LDFLAGS = -mwindows
endif

# Misc stuff

error_DATA = shell.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

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
	shell.error.xml				\
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

BUILT_SOURCES = $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

noinst_SCRIPTS = evolution-nognome

evolution-icon.o: evolution.ico evolution-mail.ico evolution-memos.ico evolution-tasks.ico
	$(WINDRES) evolution-icon.rc evolution-icon.o

-include $(top_srcdir)/git.mk

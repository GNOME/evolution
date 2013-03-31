# Executable
bin_PROGRAMS = evolution

# Shell library
privsolib_LTLIBRARIES = libeshell.la

eshellincludedir = $(privincludedir)/shell

ENUM_TYPES = e-shell.h

e-shell-enumtypes.h: e-shell-enumtypes.h.template $(ENUM_TYPES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template e-shell-enumtypes.h.template $(ENUM_TYPES)) > $@

e-shell-enumtypes.c: e-shell-enumtypes.c.template $(ENUM_TYPES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template e-shell-enumtypes.c.template $(ENUM_TYPES)) > $@

ENUM_GENERATED = e-shell-enumtypes.h e-shell-enumtypes.c

eshellinclude_HEADERS = 			\
	e-shell.h				\
	e-shell-backend.h			\
	e-shell-common.h			\
	e-shell-content.h			\
	e-shell-enumtypes.h			\
	e-shell-searchbar.h			\
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
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)					\
	-I$(top_builddir)/shell					\
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
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(CLUTTER_GTK_CFLAGS)

libeshell_la_SOURCES =				\
	$(eshellinclude_HEADERS)		\
	e-shell.c				\
	e-shell-backend.c			\
	e-shell-content.c			\
	e-shell-enumtypes.c			\
	e-shell-searchbar.c			\
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
	evo-version.h				\
	es-event.c

libeshell_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libeshell_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(CLUTTER_GTK_LIBS)

# Evolution executable

if HAVE_WINDRES
EVOLUTIONICON = evolution-icon.o
endif

evolution_CPPFLAGS =						\
	$(AM_CPPFLAGS)						\
	$(NM_CPPFLAGS)						\
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
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DPREFIX=\""$(prefix)"\"				\
	-DEVOLUTION_MX_THEMEDIR=\"$(privdatadir)/theme\" 	\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBDIR=\""$(datadir)"\"				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(CLUTTER_GTK_CFLAGS)

evolution_SOURCES =				\
	main.c					\
	e-convert-local-mail.c			\
	e-migrate-base-dirs.c

evolution_LDADD =							\
	libeshell.la							\
	$(top_builddir)/e-util/libeutil.la				\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(CLUTTER_GTK_LIBS)						\
	$(EVOLUTIONICON)

if OS_WIN32
evolution_LDFLAGS = -mwindows
endif

# Killev utility

if OS_WIN32
else
privlibexec_PROGRAMS = killev
endif

killev_CPPFLAGS =						\
	$(AM_CPPFLAGS)						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)

killev_SOURCES = killev.c

killev_LDADD =							\
	$(GNOME_PLATFORM_LIBS)					\
	$(EVOLUTION_DATA_SERVER_LIBS)

# Misc stuff

error_DATA = shell.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

# Extra dist stuff

EXTRA_DIST = 					\
	evo-version.h.in			\
	e-shell-enumtypes.h.template		\
	e-shell-enumtypes.c.template		\
	shell.error.xml				\
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

BUILT_SOURCES = \
	$(error_DATA) \
	$(ENUM_GENERATED) \
	evo-version.h
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

evolution-icon.o: evolution.ico evolution-mail.ico evolution-memos.ico evolution-tasks.ico
	$(WINDRES) evolution-icon.rc evolution-icon.o

-include $(top_srcdir)/git.mk

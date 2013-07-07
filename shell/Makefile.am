# Executable
bin_PROGRAMS = evolution

# Shell library
privsolib_LTLIBRARIES = libevolution-shell.la

evolution_shell_includedir = $(privincludedir)/shell

ENUM_TYPES = e-shell.h

e-shell-enumtypes.h: $(top_srcdir)/enumtypes.h.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.h.template \
		--fhead "#ifndef E_SHELL_ENUMTYPES_H\n#define E_SHELL_ENUMTYPES_H\n" \
		--ftail "#endif /* E_SHELL_ENUMTYPES_H */\n" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

e-shell-enumtypes.c: $(top_srcdir)/enumtypes.c.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.c.template \
		--fhead "#include \"e-shell-enumtypes.h\"" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

ENUM_GENERATED = e-shell-enumtypes.h e-shell-enumtypes.c

evolution_shell_include_HEADERS =		\
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
	$(NULL)

libevolution_shell_la_CPPFLAGS =				\
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
	$(CLUTTER_GTK_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

libevolution_shell_la_SOURCES =			\
	$(evolution_shell_include_HEADERS)	\
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
	$(NULL)

libevolution_shell_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolution_shell_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la	\
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
	$(CLUTTER_GTK_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

evolution_SOURCES =				\
	main.c					\
	e-convert-local-mail.c			\
	e-migrate-base-dirs.c

evolution_LDADD =							\
	libevolution-shell.la						\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(CLUTTER_GTK_LIBS)						\
	$(EVOLUTIONICON)

evolution_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

if OS_WIN32
evolution_LDFLAGS += -mwindows
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
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)

killev_SOURCES = killev.c

killev_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

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

SUBDIRS = glade

INCLUDES = 								\
	-I$(top_srcdir)/widgets						\
	-I$(top_srcdir)/widgets/misc					\
	-I$(top_srcdir)							\
	$(BONOBO_GNOME_CFLAGS)						\
	-DEVOLUTION_IMAGES=\""$(datadir)/images/evolution"\"		\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"				\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"   		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            		\
	-DEVOLUTION_GLADEDIR=\"$(datadir)/evolution/glade\"		\
	-DG_LOG_DOMAIN=\"evolution-shell\"

EXTRA_DIST = $(IDLS) ui.xml

CLEANFILES = $(IDL_GENERATED)

# CORBA stuff

IDLS = \
	Evolution.idl			\
	Evolution-LocalStorage.idl	\
	Evolution-Session.idl		\
	Evolution-Shell.idl		\
	Evolution-ShellComponent.idl	\
	Evolution-ShellView.idl		\
	Evolution-Storage.idl

IDL_GENERATED = \
	Evolution.h		\
	Evolution-common.c	\
	Evolution-skels.c	\
	Evolution-stubs.c

Evolution-impl.o: Evolution.h

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I$(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		-I$(srcdir) $(srcdir)/Evolution.idl

# Shell library

noinst_LIBRARIES =	\
	libeshell.a

libeshell_a_SOURCES = 				\
	$(IDL_GENERATED)			\
	e-folder-tree.c				\
	e-folder-tree.h				\
	evolution-local-storage.c		\
	evolution-local-storage.h		\
	evolution-session.c			\
	evolution-session.h			\
	evolution-shell-client.c		\
	evolution-shell-client.h		\
	evolution-shell-component-client.c	\
	evolution-shell-component-client.h	\
	evolution-shell-component.c		\
	evolution-shell-component.h		\
	evolution-shell-view.c			\
	evolution-shell-view.h			\
	evolution-storage-listener.c		\
	evolution-storage-listener.h		\
	evolution-storage.c			\
	evolution-storage.h

# Evolution executable

bin_PROGRAMS = evolution

evolution_SOURCES =				\
	e-component-registry.c			\
	e-component-registry.h			\
	e-corba-storage-registry.c		\
	e-corba-storage-registry.h		\
	e-corba-storage.c			\
	e-corba-storage.h			\
	e-folder-type-registry.c		\
	e-folder-type-registry.h		\
	e-folder.c				\
	e-folder.h				\
	e-local-folder.c			\
	e-local-folder.h			\
	e-local-storage.c			\
	e-local-storage.h			\
	e-setup.c				\
	e-setup.h				\
	e-shell-constants.h			\
	e-shell-folder-creation-dialog.c	\
	e-shell-folder-creation-dialog.h	\
	e-shell-folder-selection-dialog.c	\
	e-shell-folder-selection-dialog.h	\
	e-shell-folder-title-bar.c		\
	e-shell-folder-title-bar.h		\
	e-shell-utils.c				\
	e-shell-utils.h				\
	e-shell-view-menu.c			\
	e-shell-view-menu.h			\
	e-shell-view.c				\
	e-shell-view.h				\
	e-shell.c				\
	e-shell.h				\
	e-shortcuts-view.c			\
	e-shortcuts-view.h			\
	e-shortcuts.c				\
	e-shortcuts.h				\
	e-storage-set-view.c			\
	e-storage-set-view.h			\
	e-storage-set.c				\
	e-storage-set.h				\
	e-storage.c				\
	e-storage.h				\
	main.c

evolution_LDADD =						\
	libeshell.a						\
	$(top_builddir)/widgets/shortcut-bar/libshortcut-bar.a  \
	$(top_builddir)/widgets/e-paned/libepaned.a		\
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(top_builddir)/widgets/e-table/libetable.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a		\
	$(top_builddir)/e-util/libeutil.la			\
	$(UNICODE_LIBS)						\
	$(BONOBO_GNOME_LIBS)

uidir = $(datadir)/evolution/shell
ui_DATA = ui.xml

# Purify support

if ENABLE_PURIFY

PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution.pure

evolution.pure: evolution
	@rm -f evolution.pure
	$(PLINK) $(evolution_LDFLAGS) $(evolution_OBJECTS) $(evolution_LDADD) $(LIBS)

endif

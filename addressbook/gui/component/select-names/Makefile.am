CPPFLAGS =					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"	\
	$(EXTRA_GNOME_CFLAGS)				\
	$(GNOME_INCLUDEDIR)				\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/e-table			\
	-I$(top_srcdir)/addressbook/gui/minicard	\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"

lib_LTLIBRARIES = libeselectnames.la

libeselectnames_la_SOURCES = 		\
	e-select-names-entry.c		\
	e-select-names-entry.h		\
	e-select-names-manager.c	\
	e-select-names-manager.h	\
	e-select-names-model.c		\
	e-select-names-model.h		\
	e-select-names-table-model.c	\
	e-select-names-table-model.h	\
	e-select-names-text-model.c	\
	e-select-names-text-model.h	\
	e-select-names.c		\
	e-select-names.h


gladedir   = $(datadir)/evolution/glade
glade_DATA = select-names.glade

EXTRA_DIST = 			\
	$(glade_DATA)		\
	select-names.glade.h


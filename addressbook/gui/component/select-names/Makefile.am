# CORBA stuff

IDLS = \
	Evolution-Addressbook-SelectNames.idl

IDL_GENERATED = \
	Evolution-Addressbook-SelectNames.h		\
	Evolution-Addressbook-SelectNames-common.c	\
	Evolution-Addressbook-SelectNames-skels.c	\
	Evolution-Addressbook-SelectNames-stubs.c

Evolution-Addressbook-SelectNames-impl.o: Evolution-Addressbook-SelectNames.h

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		$(srcdir)/Evolution-Addressbook-SelectNames.idl

oafdir = $(datadir)/oaf

oaf_in_files = GNOME_Evolution_Addressbook_SelectNames.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@


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
	-I$(top_srcdir)/addressbook/gui/widgets		\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"

lib_LTLIBRARIES = libeselectnames.la

libeselectnames_la_SOURCES = 		\
	$(IDL_GENERATED)		\
	e-select-names-bonobo.c		\
	e-select-names-bonobo.h		\
	e-select-names-completion.c	\
	e-select-names-completion.h	\
	e-select-names-factory.c	\
	e-select-names-factory.h	\
	e-select-names-manager.c	\
	e-select-names-manager.h	\
	e-select-names-model.c		\
	e-select-names-model.h		\
	e-select-names-popup.c		\
	e-select-names-popup.h		\
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
	$(oaf_in_files)		\
	$(oaf_DATA)		\
	$(IDLS)

BUILT_SOURCES = $(IDL_GENERATED)
CLEANFILES   += $(BUILT_SOURCES)

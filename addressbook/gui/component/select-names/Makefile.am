# CORBA stuff

idl_DATA = \
	Evolution-Addressbook-SelectNames.idl

IDL_GENERATED_H = 					\
	Evolution-Addressbook-SelectNames.h
IDL_GENERATED_C = 					\
	Evolution-Addressbook-SelectNames-common.c	\
	Evolution-Addressbook-SelectNames-skels.c	\
	Evolution-Addressbook-SelectNames-stubs.c
IDL_GENERATED = $(IDL_GENERATED_C) $(IDL_GENERATED_H)

$(IDL_GENERATED_H): $(idl_DATA)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) \
		$(srcdir)/Evolution-Addressbook-SelectNames.idl
$(IDL_GENERATED_C): $(IDL_GENERATED_H)

server_in_files = GNOME_Evolution_Addressbook_SelectNames.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(BASE_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/addressbook/gui/minicard	\
	-I$(top_srcdir)/addressbook/gui/widgets		\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/util		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

privlib_LTLIBRARIES = libeselectnames.la

libeselectnames_la_SOURCES = 			\
	$(IDL_GENERATED)			\
	$(MARSHAL_GENERATED)			\
	e-select-names-bonobo.c			\
	e-select-names-bonobo.h			\
	e-select-names-completion.c		\
	e-select-names-completion.h		\
	e-select-names-config.c			\
	e-select-names-config.h			\
	e-select-names-config-keys.h		\
	e-select-names-manager.c		\
	e-select-names-manager.h		\
	e-select-names-model.c			\
	e-select-names-model.h			\
	e-select-names-popup.c			\
	e-select-names-popup.h			\
	e-select-names-table-model.c		\
	e-select-names-table-model.h		\
	e-select-names-text-model.c		\
	e-select-names-text-model.h		\
	e-select-names.c			\
	e-select-names.h

libeselectnames_la_LIBADD =					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

# We'll need these again when we split out the select names control
# into its own shlib factory, but for now they're redundant.
#
#	e-select-names-factory.c
#	e-select-names-factory.h

MARSHAL_GENERATED = e-select-names-marshal.c e-select-names-marshal.h
@EVO_MARSHAL_RULE@

glade_DATA = select-names.glade
etspec_DATA = e-select-names.etspec e-select-names-section.etspec

EXTRA_DIST = 			\
	$(glade_DATA)		\
	$(server_in_files)	\
	$(idl_DATA)		\
	$(etspec_DATA)		\
	e-select-names-marshal.list

BUILT_SOURCES = $(IDL_GENERATED) $(MARSHAL_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

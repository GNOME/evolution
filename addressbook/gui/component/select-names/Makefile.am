# CORBA stuff

idl_DATA = \
	Evolution-Addressbook-SelectNames.idl

IDL_GENERATED = \
	Evolution-Addressbook-SelectNames.h		\
	Evolution-Addressbook-SelectNames-common.c	\
	Evolution-Addressbook-SelectNames-skels.c	\
	Evolution-Addressbook-SelectNames-stubs.c

Evolution-Addressbook-SelectNames-impl.o: Evolution-Addressbook-SelectNames.h

$(IDL_GENERATED): $(idl_DATA)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) \
		$(srcdir)/Evolution-Addressbook-SelectNames.idl

server_in_files = GNOME_Evolution_Addressbook_SelectNames.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@COMPONENTDIR\@|$(componentdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"	\
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
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"	\
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DLIBGNOME_DISABLE_DEPRECATED			\
	-DLIBGNOMEUI_DISABLE_DEPRECATED			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

privlib_LTLIBRARIES = libeselectnames.la

libeselectnames_la_SOURCES = 			\
	$(IDL_GENERATED)			\
	e-select-names-bonobo.c			\
	e-select-names-bonobo.h			\
	e-select-names-completion.c		\
	e-select-names-completion.h		\
	e-select-names-manager.c		\
	e-select-names-manager.h		\
	e-select-names-marshal.c		\
	e-select-names-model.c			\
	e-select-names-model.h			\
	e-select-names-popup.c			\
	e-select-names-popup.h			\
	e-select-names-table-model.c		\
	e-select-names-table-model.h		\
	e-select-names-text-model.c		\
	e-select-names-text-model.h		\
	e-select-names.c			\
	e-select-names.h			\
	e-simple-card-bonobo.c			\
	e-simple-card-bonobo.h

# We'll need these again when we split out the select names control
# into its own shlib factory, but for now they're redundant.
#
#	e-select-names-factory.c
#	e-select-names-factory.h

# GLib marshalling cruft

e-select-names-marshal.h: e-select-names-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=e_select_names_marshal e-select-names-marshal.list --header > e-select-names-marshal.tmp \
	&& mv e-select-names-marshal.tmp e-select-names-marshal.h ) \
	|| ( rm -f e-select-names-marshal.tmp && exit 1 )

e-select-names-marshal.c: e-select-names-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=e_select_names_marshal e-select-names-marshal.list --body > e-select-names-marshal.tmp \
	&& mv e-select-names-marshal.tmp e-select-names-marshal.c ) \
	|| ( rm -f e-select-names-marshal.tmp && exit 1 )

MARSHAL_GENERATED = e-select-names-marshal.c e-select-names-marshal.h

gladedir   = $(datadir)/evolution-$(BASE_VERSION)/glade
glade_DATA = select-names.glade

etspecdir   = $(datadir)/evolution-$(BASE_VERSION)/etspec
etspec_DATA = e-select-names.etspec

EXTRA_DIST = 			\
	$(glade_DATA)		\
	$(server_in_files)	\
	$(server_DATA)		\
	$(idl_DATA)		\
	$(etspec_DATA)		\
	e-select-names-marshal.list

BUILT_SOURCES = $(IDL_GENERATED) $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook/		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/addressbook/backend	\
	-I$(top_srcdir)/addressbook/gui/merging	\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_builddir)/shell			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"\
	-DDATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"	\
	-DEVOLUTIONDIR=\""$(evolutiondir)"\"	\
	-DG_LOG_DOMAIN=\"contact-list-editor\"	\
	-DG_DISABLE_DEPRECATED			\
	-DGTK_DISABLE_DEPRECATED		\
	-DLIBGNOME_DISABLE_DEPRECATED		\
	-DLIBGNOMEUI_DISABLE_DEPRECATED		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LIBRARIES =				\
	libecontactlisteditor.a

libecontactlisteditor_a_SOURCES = 		\
	e-contact-list-editor.c			\
	e-contact-list-editor.h			\
	e-contact-list-editor-marshal.c		\
	e-contact-list-model.c			\
	e-contact-list-model.h

# GLib marshalling cruft

e-contact-list-editor-marshal.h: e-contact-list-editor-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=ecle_marshal e-contact-list-editor-marshal.list --header > e-contact-list-editor-marshal.tmp \
	&& mv e-contact-list-editor-marshal.tmp e-contact-list-editor-marshal.h ) \
	|| ( rm -f e-contact-list-editor-marshal.tmp && exit 1 )

e-contact-list-editor-marshal.c: e-contact-list-editor-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=ecle_marshal e-contact-list-editor-marshal.list --body > e-contact-list-editor-marshal.tmp \
	&& mv e-contact-list-editor-marshal.tmp e-contact-list-editor-marshal.c ) \
	|| ( rm -f e-contact-list-editor-marshal.tmp && exit 1 )

MARSHAL_GENERATED = e-contact-list-editor-marshal.c e-contact-list-editor-marshal.h


iconsdir = $(datadir)/images/evolution

gladedir   = $(datadir)/evolution/glade

glade_DATA = 					\
	contact-list-editor.glade

etspecdir   = $(datadir)/evolution/etspec
etspec_DATA = e-contact-list-editor.etspec

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(glade_DATA)	\
	     $(etspec_DATA)	\
	     e-contact-list-editor-marshal.list

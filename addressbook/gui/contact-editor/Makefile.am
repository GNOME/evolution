INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook/		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/addressbook/backend	\
	-I$(top_srcdir)/addressbook/gui/merging	\
	-I$(top_srcdir)/widgets/e-table		\
	-I$(top_builddir)/shell			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DDATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"	\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"	\
	-DEVOLUTIONDIR=\""$(evolutiondir)"\"	\
	-DG_LOG_DOMAIN=\"contact-editor\"	\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LIBRARIES =				\
	libecontacteditor.a

libecontacteditor_a_SOURCES = 			\
	e-contact-editor-address.c		\
	e-contact-editor-address.h		\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor-marshal.c		\
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-save-as.c			\
	e-contact-save-as.h			\
	e-contact-quick-add.c			\
	e-contact-quick-add.h

# GLib marshalling cruft

e-contact-editor-marshal.h: e-contact-editor-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=ece_marshal e-contact-editor-marshal.list --header > e-contact-editor-marshal.tmp \
	&& mv e-contact-editor-marshal.tmp e-contact-editor-marshal.h ) \
	|| ( rm -f e-contact-editor-marshal.tmp && exit 1 )

e-contact-editor-marshal.c: e-contact-editor-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=ece_marshal e-contact-editor-marshal.list --body > e-contact-editor-marshal.tmp \
	&& mv e-contact-editor-marshal.tmp e-contact-editor-marshal.c ) \
	|| ( rm -f e-contact-editor-marshal.tmp && exit 1 )

MARSHAL_GENERATED = e-contact-editor-marshal.c e-contact-editor-marshal.h

evolutiondir = $(datadir)/evolution

evolution_DATA = arrow.png

iconsdir = $(datadir)/images/evolution

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	contact-editor.glade		\
	fulladdr.glade			\
	fullname.glade			\
	file-exists.glade		\
	e-contact-editor-confirm-delete.glade

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST=	$(evolution_DATA)		\
		$(glade_DATA)			\
		e-contact-editor-marshal.list

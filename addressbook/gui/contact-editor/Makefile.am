INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/widgets/e-table			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/camel				\
	-I$(top_builddir)/camel				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)\""	\
	-DPREFIX=\""$(prefix)"\"			\
	-DG_LOG_DOMAIN=\"contact-editor\"		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libecontacteditor.la

libecontacteditor_la_SOURCES = 			\
	$(MARSHAL_GENERATED)			\
	eab-editor.c				\
	eab-editor.h				\
	e-contact-editor-im.c			\
	e-contact-editor-im.h			\
	e-contact-editor-address.c		\
	e-contact-editor-address.h		\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-quick-add.c			\
	e-contact-quick-add.h

MARSHAL_GENERATED = e-contact-editor-marshal.c e-contact-editor-marshal.h
@EVO_MARSHAL_RULE@

glade_DATA = 				\
	im.glade				\
	contact-editor.glade		\
	fulladdr.glade			\
	fullname.glade

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST=	$(glade_DATA)		\
		e-contact-editor-marshal.list

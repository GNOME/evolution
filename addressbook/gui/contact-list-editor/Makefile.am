INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/			\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_builddir)/shell				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)\""	\
	-DPREFIX=\""$(prefix)"\"			\
	-DG_LOG_DOMAIN=\"contact-list-editor\"		\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

privlib_LTLIBRARIES =				\
	libecontactlisteditor.la

libecontactlisteditor_la_SOURCES = 		\
	$(MARSHAL_GENERATED)			\
	e-contact-list-editor.c			\
	e-contact-list-editor.h			\
	e-contact-list-model.c			\
	e-contact-list-model.h

MARSHAL_GENERATED = e-contact-list-editor-marshal.c e-contact-list-editor-marshal.h
@EVO_MARSHAL_RULE@

glade_DATA  = contact-list-editor.glade
etspec_DATA = e-contact-list-editor.etspec

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(glade_DATA)	\
	     $(etspec_DATA)	\
	     e-contact-list-editor-marshal.list

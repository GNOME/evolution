INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook/		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/addressbook/backend	\
	-I$(top_srcdir)/addressbook/gui/merging	\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_builddir)/shell			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DDATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"	\
	-DEVOLUTIONDIR=\""$(evolutiondir)"\"	\
	-DG_LOG_DOMAIN=\"contact-list-editor\"	\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LIBRARIES =				\
	libecontactlisteditor.a

libecontactlisteditor_a_SOURCES = 		\
	e-contact-list-editor.c			\
	e-contact-list-editor.h			\
	e-contact-list-model.c			\
	e-contact-list-model.h

gladedir   = $(datadir)/evolution/glade

glade_DATA = 					\
	contact-list-editor.glade

EXTRA_DIST = $(glade_DATA)

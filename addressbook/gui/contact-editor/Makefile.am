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
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-save-as.c			\
	e-contact-save-as.h			\
	e-contact-quick-add.c			\
	e-contact-quick-add.h

evolutiondir = $(datadir)/evolution

evolution_DATA = arrow.png

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	contact-editor.glade		\
	fulladdr.glade			\
	fullname.glade			\
	file-exists.glade		\
	e-contact-editor-confirm-delete.glade

EXTRA_DIST = $(evolution_DATA)		\
	$(glade_DATA)

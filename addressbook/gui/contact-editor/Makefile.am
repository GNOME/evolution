CPPFLAGS =					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DDATADIR=\""$(datadir)"\"		\
	-DEVOLUTIONDIR=\""$(evolutiondir)"\"

INCLUDES =					\
	$(BONOBO_GNOME_CFLAGS)			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook/		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_srcdir)/widgets/e-table		\
	-DG_LOG_DOMAIN=\"contact-editor\"

noinst_LIBRARIES =				\
	libecontacteditor.a

libecontacteditor_a_SOURCES = 			\
	e-contact-editor-address.c		\
	e-contact-editor-address.h		\
	e-contact-editor-categories.c		\
	e-contact-editor-categories.h		\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor.c			\
	e-contact-editor.h			\
	e-contact-save-as.c			\
	e-contact-save-as.h

noinst_PROGRAMS =		\
	contact-editor-test

contact_editor_test_SOURCES = 	\
	test-editor.c

contact_editor_test_LDADD =					\
	libecontacteditor.a					\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/addressbook/printing/libecontactprint.a	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/addressbook/ename/libename.la		\
	$(top_builddir)/libversit/libversit.la			\
	$(GNOMEGNORBA_LIBS)					\
	$(BONOBO_GNOME_LIBS)				\
	$(top_builddir)/widgets/e-table/libetable.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(EXTRA_GNOME_LIBS)

evolutiondir = $(datadir)/evolution

evolution_DATA = arrow.png

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	contact-editor.glade		\
	fulladdr.glade			\
	fullname.glade			\
	categories.glade		\
	e-contact-editor-confirm-delete.glade

EXTRA_DIST = $(evolution_DATA)		\
	$(glade_DATA)			\
	e-contact-editor-strings.h	\
	fulladdr.glade.h		\
	fullname-strings.h		\
	categories-strings.h		\
	e-contact-editor-confirm-delete.glade.h

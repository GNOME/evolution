CPPFLAGS =					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DDATADIR=\""$(datadir)"\"

INCLUDES =					\
	$(GNOME_INCLUDEDIR)			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/addressbook/		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_srcdir)/widgets/e-table		\
	-DG_LOG_DOMAIN=\"contact-editor\"

noinst_LIBRARIES =				\
	libecontacteditor.a

libecontacteditor_a_SOURCES = 			\
	e-contact-editor-categories.c		\
	e-contact-editor-categories.h		\
	e-contact-editor-fullname.c		\
	e-contact-editor-fullname.h		\
	e-contact-editor.c			\
	e-contact-editor.h

noinst_PROGRAMS =		\
	contact-editor-test

contact_editor_test_SOURCES = 	\
	test-editor.c

contact_editor_test_LDADD =					\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/addressbook/ename/libename.la		\
	$(top_builddir)/libversit/libversit.la			\
	$(GNOMEGNORBA_LIBS)					\
	-lbonobo						\
	$(top_builddir)/e-util/libeutil.la			\
	libecontacteditor.a					\
	$(top_builddir)/widgets/e-table/libetable.a		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/e-text/libetext.a		\
	$(EXTRA_GNOME_LIBS)

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	contact-editor.glade		\
	fullname.glade			\
	categories.glade

EXTRA_DIST = $(glade_DATA)		\
	e-contact-editor-strings.h	\
	fullname-strings.h		\
	categories-strings.h

ecpsdir    = $(privdatadir)/ecps
ecps_DATA  =				\
	smallbook.ecps			\
	medbook.ecps			\
	phonelist.ecps

glade_DATA = 				\
	e-contact-print.glade

INCLUDES =					\
	$(GNOME_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"addressbook-printing\"	\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)/addressbook/backend	\
	-I$(top_builddir)/addressbook/backend	\
	-I$(top_srcdir)				\
        -DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"	\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libecontactprint.la

libecontactprint_la_SOURCES = 			\
	e-contact-print-envelope.c		\
	e-contact-print-envelope.h		\
	e-contact-print-style-editor.c		\
	e-contact-print-style-editor.h		\
	e-contact-print-types.h			\
	e-contact-print.c			\
	e-contact-print.h

noinst_PROGRAMS =			\
	contact-print-test		\
	contact-print-style-editor-test

contact_print_test_SOURCES = 	\
	test-print.c

contact_print_test_LDADD =	\
	libecontactprint.la					\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

contact_print_style_editor_test_SOURCES = 	\
	test-contact-print-style-editor.c

contact_print_style_editor_test_LDADD =	\
	libecontactprint.la					\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(EVOLUTION_ADDRESSBOOK_LIBS)



EXTRA_DIST = 		\
	$(glade_DATA)	\
	$(ecps_DATA)

ecpsdir    = $(datadir)/evolution/ecps

ecps_DATA  =				\
	smallbook.ecps			\
	medbook.ecps			\
	phonelist.ecps

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	e-contact-print.glade

INCLUDES =					\
	$(GNOME_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"addressbook-printing\"	\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)				\
        -DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"	\
	-DDATADIR=\""$(datadir)"\"		\
	$(GNOME_PRINT_CFLAGS)			\
        $(BONOBO_GNOME_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)

noinst_LIBRARIES =				\
	libecontactprint.a

libecontactprint_a_SOURCES = 			\
	e-contact-print-envelope.c		\
	e-contact-print-envelope.h		\
	e-contact-print-style-editor.c		\
	e-contact-print-style-editor.h		\
	e-contact-print-types.h			\
	e-contact-print.c			\
	e-contact-print.h

noinst_PROGRAMS =	\
	contact-print-test	\
	contact-print-style-editor-test

contact_print_test_SOURCES = 	\
	test-print.c

contact_print_test_LDADD =	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/ename/libename.la			\
	libecontactprint.a	\
	$(EXTRA_GNOME_LIBS)	\
	$(BONOBO_GNOME_LIBS)    \
	$(GNOME_PRINT_LIBS) 

contact_print_style_editor_test_SOURCES = 	\
	test-contact-print-style-editor.c

contact_print_style_editor_test_LDADD =	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/ename/libename.la			\
	libecontactprint.a	\
	$(EXTRA_GNOME_LIBS)	\
	$(BONOBO_GNOME_LIBS)    \
	$(GNOME_PRINT_LIBS)

EXTRA_DIST = \
	$(glade_DATA)	\
	$(ecps_DATA)	\
	e-contact-print.glade.h

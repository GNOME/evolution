ecpsdir    = $(datadir)/evolution/ecps

ecps_DATA  =				\
	smallbook.ecps			\
	medbook.ecps			\
	phonelist.ecps

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	e-contact-print.glade		\
	e-contact-print.glade.h

CPPFLAGS = \
        -DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\"	\
	-DDATADIR=\""$(datadir)"\"		\
	$(GNOME_PRINT_CFLAGS)			\
        $(BONOBO_GNOME_CFLAGS)

INCLUDES =					\
	$(GNOME_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"addressbook-printing\"	\
	-I$(top_srcdir)/addressbook		\
	-I$(top_srcdir)

noinst_LIBRARIES =				\
	libecontactprint.a

libecontactprint_a_SOURCES = 			\
	e-contact-print.c			\
	e-contact-print.h			\
	e-contact-print-style-editor.c		\
	e-contact-print-style-editor.h		\
	e-contact-print-types.h

noinst_PROGRAMS =	\
	contact-print-test	\
	contact-print-style-editor-test

contact_print_test_SOURCES = 	\
	test-print.c

contact_print_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	$(BONOBO_GNOME_LIBS)    \
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/addressbook/ename/libename.la			\
	libecontactprint.a	\
	$(GNOME_PRINT_LIBS) 

contact_print_style_editor_test_SOURCES = 	\
	test-contact-print-style-editor.c

contact_print_style_editor_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	$(BONOBO_GNOME_LIBS)    \
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/addressbook/ename/libename.la			\
	libecontactprint.a	\
	$(GNOME_PRINT_LIBS)

EXTRA_DIST = $(glade_DATA) $(ecps_DATA)

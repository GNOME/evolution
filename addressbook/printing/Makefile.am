
CPPFLAGS = \
        -DEVOLUTION_GLADEDIR=\""$(gladedir)"\"	\
	-DDATADIR=\""$(datadir)"\"		\
	$(GNOME_PRINT_CFLAGS)

INCLUDES =					\
	$(GNOME_INCLUDEDIR)

noinst_LIBRARIES =				\
	libecontactprint.a

libecontactprint_a_SOURCES = 			\
	e-contact-print.c			\
	e-contact-print.h			\
	e-contact-print-types.h

noinst_PROGRAMS =	\
	contact-print-test

contact_print_test_SOURCES = 	\
	test-print.c

contact_print_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libecontactprint.a	\
	$(GNOME_PRINT_LIBS)

gladedir   = $(datadir)/evolution/glade

#glade_DATA = 				\
#	e-contact-print.glade



images = email.png head.png phone.png snailmail.png web.png arrow.png briefcase.png netmeeting.png netfreebusy.png

EXTRA_DIST = $(images)
pkgdata_DATA = $(images)

CPPFLAGS = \
        -DEVOLUTION_GLADEDIR=\""$(gladedir)"\"           \
	-DDATADIR=\""$(datadir)"\"

INCLUDES =					\
	$(GNOME_INCLUDEDIR)

noinst_LIBRARIES =				\
	libecontacteditor.a

libecontacteditor_a_SOURCES = 			\
	e-contact-editor.c			\
	e-contact-editor.h

noinst_PROGRAMS =	\
	contact-editor-test

contact_editor_test_SOURCES = 	\
	test-editor.c

contact_editor_test_LDADD =	\
	libecontacteditor.a	\
	$(EXTRA_GNOME_LIBS)

gladedir   = $(datadir)/evolution/glade

glade_DATA = 				\
	contact-editor.glade


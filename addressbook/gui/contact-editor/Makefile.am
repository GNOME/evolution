
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
	$(EXTRA_GNOME_LIBS)	\
	libecontacteditor.a	

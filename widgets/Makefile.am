
SUBDIRS =	\
	meeting-time-sel shortcut-bar e-table

INCLUDES =					\
	-I$(top_srcdir)/camel			\
	-I$(top_builddir)/camel			\
	$(GNOME_INCLUDEDIR)

noinst_LIBRARIES =				\
	libevolutionwidgets.a

libevolutionwidgets_a_SOURCES = 		\
	e-cursors.c				\
	e-cursors.h				\
	e-minicard.c				\
	e-minicard.h				\
	e-minicard-label.c			\
	e-minicard-label.h			\
	e-text.c				\
	e-text.h				\
	e-text-event-processor.c		\
	e-text-event-processor.h		\
	e-text-event-processor-emacs-like.c	\
	e-text-event-processor-emacs-like.h

noinst_PROGRAMS =	\
	minicard-label-test	\
	minicard-test

minicard_label_test_SOURCES = 	\
	test-minicard-label.c

minicard_label_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libevolutionwidgets.a	

minicard_test_SOURCES = 	\
	test-minicard.c

minicard_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libevolutionwidgets.a	

INCLUDES =					\
	$(GNOME_INCLUDEDIR)

noinst_LIBRARIES =				\
	libetext.a

libetext_a_SOURCES = 		\
	e-text.c				\
	e-text.h				\
	e-text-event-processor.c		\
	e-text-event-processor.h		\
	e-text-event-processor-emacs-like.c	\
	e-text-event-processor-emacs-like.h

noinst_PROGRAMS =	\
 	e-text-test

e_text_test_SOURCES =	\
 	e-text-test.c

e_text_test_LDADD =				\
	libetext.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/e-util/libeutil.la

e_text_test_LDFLAGS = `gnome-config --libs gdk_pixbuf`

INCLUDES =					\
	-I$(top_srcdir)				\
	$(GNOME_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"e-text\"		\
	$(UNICODE_CFLAGS)

noinst_LIBRARIES =				\
	libetext.a

libetext_a_SOURCES = 		\
	e-entry.c				\
	e-entry.h				\
	e-text-event-processor-emacs-like.c	\
	e-text-event-processor-emacs-like.h	\
	e-text-event-processor-types.h		\
	e-text-event-processor.c		\
	e-text-event-processor.h		\
	e-text-model.c				\
	e-text-model.h				\
	e-text.c				\
	e-text.h

noinst_PROGRAMS =	\
	e-text-test	\
	e-entry-test

e_text_test_SOURCES =	\
	e-text-test.c

e_text_test_LDADD =				\
	libetext.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/e-util/libeutil.la

e_entry_test_SOURCES =	\
	e-entry-test.c

e_entry_test_LDADD =				\
	libetext.a				\
	$(EXTRA_GNOME_LIBS)			\
	$(top_builddir)/e-util/libeutil.la

e_text_test_LDFLAGS = `gnome-config --libs gdk_pixbuf`

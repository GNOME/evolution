INCLUDES =					\
	-I$(top_srcdir)/widgets/e-text		\
	-I$(top_srcdir)/e-util			\
	$(GNOME_INCLUDEDIR)

noinst_LIBRARIES =				\
	libeminicard.a

libeminicard_a_SOURCES = 		\
	e-minicard.c				\
	e-minicard.h				\
	e-minicard-label.c			\
	e-minicard-label.h			\
	e-reflow.c				\
	e-reflow.h

noinst_PROGRAMS =	\
	minicard-label-test	\
	minicard-test		\
	reflow-test

minicard_label_test_SOURCES = 	\
	test-minicard-label.c

minicard_label_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libeminicard.a		\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/widgets/e-text/libetext.a

minicard_test_SOURCES = 	\
	test-minicard.c

minicard_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libeminicard.a	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/widgets/e-text/libetext.a

reflow_test_SOURCES = 	\
	test-reflow.c

reflow_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	libeminicard.a	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/widgets/e-text/libetext.a

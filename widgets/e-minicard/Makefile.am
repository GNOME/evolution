INCLUDES =					\
	-I$(top_srcdir)/addressbook/backend/ebook	\
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
	e-minicard-view.c			\
	e-minicard-view.h			\
	e-reflow-sorted.c			\
	e-reflow-sorted.h			\
	e-reflow.c				\
	e-reflow.h

noinst_PROGRAMS =	\
	minicard-label-test	\
	minicard-test		\
	reflow-test		\
	minicard-view-test

minicard_label_test_SOURCES = 	\
	test-minicard-label.c

minicard_label_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	$(GNOMEGNORBA_LIBS)				\
	libeminicard.a		\
	-lbonobo					\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/widgets/e-text/libetext.a

minicard_test_SOURCES = 	\
	test-minicard.c

minicard_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	$(GNOMEGNORBA_LIBS)				\
	libeminicard.a	\
	-lbonobo					\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.la	\
	$(top_builddir)/widgets/e-text/libetext.a

reflow_test_SOURCES = 	\
	test-reflow.c

reflow_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	$(GNOMEGNORBA_LIBS)				\
	libeminicard.a	\
	-lbonobo					\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.la	\
	$(top_builddir)/widgets/e-text/libetext.a

minicard_view_test_SOURCES = 	\
	test-minicard-view.c

minicard_view_test_LDADD =	\
	$(EXTRA_GNOME_LIBS)	\
	$(GNOMEGNORBA_LIBS)				\
	libeminicard.a	\
	-lbonobo					\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.la	\
	$(top_builddir)/widgets/e-text/libetext.a

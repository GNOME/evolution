INCLUDES =						\
	-DG_LOG_DOMAIN=\"e-minicard\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend/ebook	\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/e-table			\
	-I$(top_srcdir)/widgets/e-reflow		\
	-I$(top_srcdir)/widgets/e-table			\
	-I$(top_srcdir)/widgets/misc			\
	$(BONOBO_GNOME_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)

noinst_LIBRARIES =				\
	libeminicard.a

libeminicard_a_SOURCES =			\
	e-addressbook-model.c			\
	e-addressbook-model.h			\
	e-addressbook-view.c			\
	e-addressbook-view.h			\
	e-minicard-control.c			\
	e-minicard-control.h			\
	e-minicard-label.c			\
	e-minicard-label.h			\
	e-minicard-view-widget.c		\
	e-minicard-view-widget.h		\
	e-minicard-view.c			\
	e-minicard-view.h			\
	e-minicard-widget.c			\
	e-minicard-widget.h			\
	e-minicard.c				\
	e-minicard.h				\
	gal-view-factory-minicard.c		\
	gal-view-factory-minicard.h		\
	gal-view-minicard.c			\
	gal-view-minicard.h

noinst_PROGRAMS =				\
	minicard-widget-test			\
	minicard-label-test			\
	minicard-test				\
	reflow-test
#	minicard-view-test

minicard_label_test_SOURCES = 	\
	test-minicard-label.c

minicard_label_test_LDADD =				\
	$(EXTRA_GNOME_LIBS)				\
	libeminicard.a					\
	$(BONOBO_GNOME_LIBS)				\
	$(top_builddir)/e-util/libeutil.la

minicard_test_SOURCES = 	\
	test-minicard.c

minicard_test_LDADD =							\
	libeminicard.a							\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/ename/libename.la			\
	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
	$(top_builddir)/addressbook/printing/libecontactprint.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a 			\
	$(top_builddir)/e-util/libeutil.la				\
	$(EXTRA_GNOME_LIBS)						\
	$(BONOBO_GNOME_LIBS)						\
	$(GNOME_PRINT_LIBS)

reflow_test_SOURCES = 	\
	test-reflow.c

reflow_test_LDADD =							\
	libeminicard.a							\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/ename/libename.la			\
	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
	$(top_builddir)/addressbook/printing/libecontactprint.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a 			\
	$(top_builddir)/e-util/libeutil.la				\
	$(EXTRA_GNOME_LIBS)						\
	$(BONOBO_GNOME_LIBS)						\
	$(GNOME_PRINT_LIBS)

#minicard_view_test_SOURCES = 	\
#	test-minicard-view.c

#minicard_view_test_LDADD =						\
#	$(EXTRA_GNOME_LIBS)						\
#	$(BONOBO_GNOME_LIBS)						\
#	libeminicard.a							\
#	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
#	$(top_builddir)/e-util/libeutil.la				\
#	$(top_builddir)/libversit/libversit.la				\
#	$(top_builddir)/addressbook/ename/libename.la			\
#	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
#	$(top_builddir)/addressbook/printing/libecontactprint.a		\
#	$(top_builddir)/widgets/misc/libemiscwidgets.a 			\
#	$(top_builddir)/e-util/libeutil.la

minicard_widget_test_SOURCES = 	\
	e-minicard-widget-test.c

minicard_widget_test_LDADD =						\
	libeminicard.a							\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/e-util/ename/libename.la			\
	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
	$(top_builddir)/addressbook/printing/libecontactprint.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a 			\
	$(top_builddir)/e-util/libeutil.la				\
	$(EXTRA_GNOME_LIBS)						\
	$(BONOBO_GNOME_LIBS)						\
	$(GNOME_PRINT_LIBS)

gladedir   = $(datadir)/evolution/glade
glade_DATA = alphabet.glade 

EXTRA_DIST = 			\
	$(glade_DATA)		\
	alphabet.glade.h

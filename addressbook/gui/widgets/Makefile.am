INCLUDES =										\
	-DG_LOG_DOMAIN=\"e-minicard\"			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/component	\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/shell				\
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DLIBGNOME_DISABLE_DEPRECATED			\
	-DLIBGNOMEUI_DISABLE_DEPRECATED			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

CORBA_COMPOSER_SOURCE_H	=			\
	Evolution-Composer.h
CORBA_COMPOSER_SOURCE_C =			\
	Evolution-Composer-common.c		\
	Evolution-Composer-skels.c		\
	Evolution-Composer-stubs.c
CORBA_COMPOSER_IDL = $(srcdir)/../../../composer/Evolution-Composer.idl

$(CORBA_COMPOSER_SOURCE_H): $(CORBA_COMPOSER_IDL)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(CORBA_COMPOSER_IDL)

$(CORBA_COMPOSER_SOURCE_C): $(CORBA_COMPOSER_SOURCE_H)

CORBA_SOURCE_H = $(CORBA_COMPOSER_SOURCE_H)
CORBA_SOURCE_C = $(CORBA_COMPOSER_SOURCE_C)
CORBA_SOURCE = $(CORBA_SOURCE_H) $(CORBA_SOURCE_C)

noinst_LTLIBRARIES =				\
	libeminicard.la

libeminicard_la_SOURCES =			\
	$(CORBA_SOURCE)				\
	e-addressbook-marshal.c			\
	e-addressbook-reflow-adapter.c		\
	e-addressbook-reflow-adapter.h		\
	e-addressbook-table-adapter.c		\
	e-addressbook-table-adapter.h		\
	e-addressbook-model.c			\
	e-addressbook-model.h			\
	e-addressbook-util.c			\
	e-addressbook-util.h			\
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

#TREEVIEW_SOURCES=	e-addressbook-treeview-adapter.c	\
#			e-addressbook-treeview-adapter.h	\
#			gal-view-factory-treeview.c		\
#			gal-view-factory-treeview.h		\
#			gal-view-treeview.c			\
#			gal-view-treeview.h

MARSHAL_GENERATED = e-addressbook-marshal.c e-addressbook-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(CORBA_SOURCE) $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

#noinst_PROGRAMS =				\
#	 minicard-widget-test			\
#	 minicard-label-test			\
#	 minicard-test
##	reflow-test
##	minicard-view-test
#
#minicard_label_test_SOURCES = 	\
#	 test-minicard-label.c
#
#minicard_label_test_LDADD =				\
#	 libeminicard.a					\
#	 $(EVOLUTION_ADDRESSBOOK_LIBS)			\
#	 $(top_builddir)/e-util/libeutil.la
#
#minicard_test_SOURCES = 	\
#	 test-minicard.c
#
#minicard_test_LDADD =							\
#	 libeminicard.a							\
#	 $(top_builddir)/addressbook/backend/ebook/libebook.la		\
#	 $(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.a \
#	 $(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.a \
#	 $(top_builddir)/addressbook/printing/libecontactprint.a		\
#	 $(top_builddir)/addressbook/gui/merging/libecardmerging.a	\
#	 $(top_builddir)/widgets/misc/libemiscwidgets.la 			\
#	 $(top_builddir)/addressbook/gui/component/select-names/libeselectnames.la	\
#	$(EVOLUTION_ADDRESSBOOK_LIBS)					\
#	 libeminicard.a
#
#reflow_test_SOURCES = 	\
#	 test-reflow.c
#
#reflow_test_LDADD =							\
#	 libeminicard.a							\
#	 $(top_builddir)/addressbook/backend/ebook/libebook.la		\
#	 $(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
#	 $(top_builddir)/addressbook/printing/libecontactprint.a		\
#	 $(top_builddir)/widgets/misc/libemiscwidgets.la 			\
#	$(EVOLUTION_ADDRESSBOOK_LIBS)

#minicard_view_test_SOURCES = 	\
#	test-minicard-view.c

#minicard_view_test_LDADD =						\
#	$(EVOLUTION_ADDRESSBOOK_LIBS)
#	libeminicard.a							\
#	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
#	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
#	$(top_builddir)/addressbook/printing/libecontactprint.a		\
#	$(top_builddir)/widgets/misc/libemiscwidgets.la
#
#minicard_widget_test_SOURCES = 	\
#	 e-minicard-widget-test.c
#
#minicard_widget_test_LDADD =						\
#	 libeminicard.a							\
#	 $(top_builddir)/addressbook/backend/ebook/libebook.la		\
#	 $(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.a \
#	 $(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.a \
#	 $(top_builddir)/addressbook/gui/component/select-names/libeselectnames.la	\
#	 $(top_builddir)/addressbook/printing/libecontactprint.a		\
#	 $(top_builddir)/widgets/misc/libemiscwidgets.la 			\
#	 $(top_builddir)/addressbook/gui/merging/libecardmerging.a	\
#	$(EVOLUTION_ADDRESSBOOK_LIBS)

etspec_DATA= e-addressbook-view.etspec

EXTRA_DIST = 			\
	$(etspec_DATA)		\
	e-addressbook-marshal.list

ruledir = $(privdatadir)

INCLUDES =										\
	-DG_LOG_DOMAIN=\"eab-widgets\"			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/component	\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/shell				\
	-DSEARCH_RULE_DIR=\"$(ruledir)\"		\
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
	libeabwidgets.la

libeabwidgets_la_SOURCES =			\
	$(CORBA_SOURCE)				\
	$(MARSHAL_GENERATED)			\
	eab-contact-display.c			\
	eab-contact-display.h			\
	eab-gui-util.c				\
	eab-gui-util.h				\
	eab-menu.c				\
	eab-menu.h				\
	eab-popup.c				\
	eab-popup.h				\
	eab-popup-control.c			\
	eab-popup-control.h			\
	eab-vcard-control.c			\
	eab-vcard-control.h			\
	e-minicard.c				\
	e-minicard.h				\
	e-minicard-label.c			\
	e-minicard-label.h			\
	e-minicard-view.c			\
	e-minicard-view.h			\
	e-minicard-view-widget.c		\
	e-minicard-view-widget.h		\
	e-addressbook-reflow-adapter.c		\
	e-addressbook-reflow-adapter.h		\
	e-addressbook-table-adapter.c		\
	e-addressbook-table-adapter.h		\
	e-addressbook-model.c			\
	e-addressbook-model.h			\
	e-addressbook-view.c			\
	e-addressbook-view.h			\
	gal-view-minicard.c			\
	gal-view-minicard.h			\
	gal-view-factory-minicard.c		\
	gal-view-factory-minicard.h

#TREEVIEW_SOURCES=	e-addressbook-treeview-adapter.c	\
#			e-addressbook-treeview-adapter.h	\
#			gal-view-factory-treeview.c		\
#			gal-view-factory-treeview.h		\
#			gal-view-treeview.c			\
#			gal-view-treeview.h

MARSHAL_GENERATED = eab-marshal.c eab-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(CORBA_SOURCE) $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

etspec_DATA= e-addressbook-view.etspec

EXTRA_DIST = 			\
	$(etspec_DATA)		\
	eab-marshal.list

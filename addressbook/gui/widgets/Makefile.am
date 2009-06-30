ruledir = $(privdatadir)
rule_DATA = addresstypes.xml

AM_CPPFLAGS =										\
	-DG_LOG_DOMAIN=\"eab-widgets\"			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DSEARCH_RULE_DIR=\"$(ruledir)\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/gui/component	\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/shell				\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

noinst_LTLIBRARIES =				\
	libeabwidgets.la

eabincludedir = $(privincludedir)/addressbook/gui/widgets

eabinclude_HEADERS = 			\
	eab-config.h			\
	eab-menu.h			\
	eab-popup.h

libeabwidgets_la_SOURCES =			\
	eab-config.c				\
	eab-contact-display.c			\
	eab-contact-display.h			\
	eab-gui-util.c				\
	eab-gui-util.h				\
	eab-menu.c				\
	eab-popup.c				\
	eab-popup.h				\
	eab-popup-control.c			\
	eab-popup-control.h			\
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

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

etspec_DATA= e-addressbook-view.etspec

EXTRA_DIST = 			\
	$(etspec_DATA)		\
	addresstypes.xml

-include $(top_srcdir)/git.mk

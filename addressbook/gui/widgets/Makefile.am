ruledir = $(privdatadir)
rule_DATA = addresstypes.xml address_formats.dat countrytransl.map

noinst_LTLIBRARIES = libeabwidgets.la

libeabwidgets_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"eab-widgets\"			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_RULEDIR=\"$(ruledir)\"		\
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"		\
	-DEVOLUTION_PRIVDATADIR=\"${privdatadir}\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/gui/merging		\
	-I$(top_srcdir)/addressbook/util		\
	-I$(top_builddir)/shell				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CHAMPLAIN_CFLAGS)				\
	$(GTKHTML_CFLAGS)

eabincludedir = $(privincludedir)/addressbook/gui/widgets

eabinclude_HEADERS = 			\
	eab-config.h

libeabwidgets_la_SOURCES =			\
	eab-config.c				\
	eab-contact-display.c			\
	eab-contact-display.h			\
	eab-contact-formatter.c			\
	eab-contact-formatter.h			\
	eab-gui-util.c				\
	eab-gui-util.h				\
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
	e-addressbook-selector.c		\
	e-addressbook-selector.h		\
	e-addressbook-view.c			\
	e-addressbook-view.h			\
	gal-view-minicard.c			\
	gal-view-minicard.h			\
	gal-view-factory-minicard.c		\
	gal-view-factory-minicard.h		\
	ea-minicard.c			\
	ea-minicard.h			\
	ea-minicard-view.c			\
	ea-minicard-view.h			\
	ea-addressbook-view.c		\
	ea-addressbook-view.h		\
	ea-addressbook.c			\
	ea-addressbook.h

libeabwidgets_la_LIBADD =					\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/e-util/libeutil.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(CHAMPLAIN_LIBS)					\
	$(GTKHTML_LIBS)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

etspec_DATA= e-addressbook-view.etspec

EXTRA_DIST = 			\
	$(etspec_DATA)		\
	$(rule_DATA)

-include $(top_srcdir)/git.mk

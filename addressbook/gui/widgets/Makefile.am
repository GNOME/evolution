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
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/util		\
	-I$(top_builddir)/shell				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CHAMPLAIN_CFLAGS)				\
	$(GEO_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

eabincludedir = $(privincludedir)/addressbook/gui/widgets

eabinclude_HEADERS = 			\
	eab-config.h

libeabwidgets_la_SOURCES =			\
	eab-config.c				\
	eab-contact-compare.c			\
	eab-contact-compare.h			\
	eab-contact-display.c			\
	eab-contact-display.h			\
	eab-contact-formatter.c			\
	eab-contact-formatter.h			\
	eab-contact-merging.c			\
	eab-contact-merging.h			\
	eab-gui-util.c				\
	eab-gui-util.h				\
	e-contact-map.c				\
	e-contact-map.h				\
	e-contact-map-window.c			\
	e-contact-map-window.h			\
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
	ea-minicard.c				\
	ea-minicard.h				\
	ea-minicard-view.c			\
	ea-minicard-view.h			\
	ea-addressbook-view.c			\
	ea-addressbook-view.h			\
	ea-addressbook.c			\
	ea-addressbook.h

libeabwidgets_la_LDFLAGS = $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libeabwidgets_la_LIBADD =					\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(CHAMPLAIN_LIBS)					\
	$(GEO_LIBS)						\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

etspec_DATA= e-addressbook-view.etspec

EXTRA_DIST = 			\
	$(etspec_DATA)		\
	$(rule_DATA)

-include $(top_srcdir)/git.mk

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/a11y/widgets/					\
	-DMAP_DIR=\""$(imagesdir)"\"					\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=__FILE__						\
	$(E_WIDGETS_CFLAGS)

privlib_LTLIBRARIES =	\
	libemiscwidgets.la

widgetsincludedir = $(privincludedir)/widgets

if ENABLE_PILOT_CONDUITS
pilot_sources = e-pilot-settings.c
pilot_headers = e-pilot-settings.h
else
pilot_sources =
pilot_headers =
endif

widgetsinclude_HEADERS =			\
	$(pilot_headers)			\
	e-activity-handler.h			\
	e-calendar.h				\
	e-calendar-item.h			\
	e-cell-date-edit.h			\
	e-cell-percent.h			\
	e-cell-renderer-combo.h			\
	e-charset-picker.h			\
	e-clipped-label.h			\
	e-combo-cell-editable.h			\
	e-config-page.h				\
	e-combo-button.h			\
	e-dateedit.h				\
	e-dropdown-button.h			\
	e-error.h				\
	e-expander.h				\
	e-image-chooser.h			\
	e-info-label.h				\
	e-map.h					\
	e-multi-config-dialog.h			\
	e-search-bar.h				\
	e-task-bar.h				\
	e-task-widget.h				\
	e-url-entry.h

libemiscwidgets_la_SOURCES =			\
	$(MARSHAL_GENERATED)			\
	$(widgetsinclude_HEADERS)		\
	$(pilot_sources)			\
	e-activity-handler.c			\
	e-calendar.c				\
	e-calendar-item.c			\
	e-cell-date-edit.c			\
	e-cell-percent.c			\
	e-cell-renderer-combo.c			\
	e-charset-picker.c			\
	e-clipped-label.c			\
	e-combo-cell-editable.c			\
	e-config-page.c				\
	e-combo-button.c			\
	e-dateedit.c				\
	e-dropdown-button.c			\
	e-error.c				\
	e-expander.c				\
	e-image-chooser.c			\
	e-info-label.c				\
	e-map.c					\
	e-multi-config-dialog.c			\
	e-search-bar.c				\
	e-task-bar.c				\
	e-task-widget.c				\
	e-url-entry.c

MARSHAL_GENERATED = e-util-marshal.c e-util-marshal.h
@EVO_MARSHAL_RULE@

error_DATA = e-system-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

libemiscwidgets_la_LIBADD = $(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/a11y/widgets/libevolution-widgets-a11y.la	\
	$(E_WIDGETS_LIBS)

noinst_LTLIBRARIES = libefilterbar.la

libefilterbar_la_SOURCES =	\
	e-filter-bar.c		\
	e-filter-bar.h

noinst_PROGRAMS = 			\
	test-calendar			\
	test-dateedit			\
	test-dropdown-button		\
	test-error			\
	test-multi-config-dialog 	\
	test-info-label

# test-calendar

test_calendar_SOURCES = 	\
	test-calendar.c

test_calendar_LDADD = 						\
	./libemiscwidgets.la					\
	$(top_builddir)/e-util/libeutil.la			\
	$(E_WIDGETS_LIBS)

# test-dateedit

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 			\
	./libemiscwidgets.la		\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)

# test-dropdown-button

test_dropdown_button_SOURCES = 	\
	test-dropdown-button.c

test_dropdown_button_LDADD =	\
	./libemiscwidgets.la	\
	$(top_builddir)/e-util/libeutil.la  \
	$(E_WIDGETS_LIBS)

# test-multi-config-dialog

test_multi_config_dialog_SOURCES = 	\
	test-multi-config-dialog.c

test_multi_config_dialog_LDADD = 	\
	./libemiscwidgets.la		\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)

# test-info-label

test_info_label_SOURCES = 	\
	test-info-label.c

test_info_label_LDADD = 		\
	./libemiscwidgets.la	\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)

test_error_SOURCES = 	\
	test-error.c

test_error_LDADD = 				\
	$(top_builddir)/e-util/libeutil.la	\
	./libemiscwidgets.la			\
	$(E_WIDGETS_LIBS)


BUILT_SOURCES = $(MARSHAL_GENERATED) $(error_i18n)
CLEANFILES    = $(BUILT_SOURCES)

EXTRA_DIST =			\
	$(error_DATA)		\
	$(error_i18n)		\
	e-pilot-settings.h	\
	e-pilot-settings.c	\
	e-util-marshal.list	\
	ChangeLog.pre-1-4

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

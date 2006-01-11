if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libfilter.la
endif

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/a11y/widgets					\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_MAIL_CFLAGS)					\
	$(EXTRA_GNOME_CFLAGS)						\
	$(ICONV_CFLAGS)


privsolib_LTLIBRARIES =		\
	libemiscwidgets.la	\
	libefilterbar.la

widgetsincludedir = $(privincludedir)/misc

if ENABLE_PILOT_CONDUITS
pilot_sources = e-pilot-settings.c
pilot_headers = e-pilot-settings.h
else
pilot_sources =
pilot_headers =
endif

glade_DATA = e-send-options.glade \
	     gal-categories.glade \
	     e-attachment.glade

widgetsinclude_HEADERS =			\
	$(pilot_headers)			\
	e-activity-handler.h			\
	e-attachment.h				\
	e-attachment-bar.h			\
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
	e-expander.h				\
	e-image-chooser.h			\
	e-info-label.h				\
	e-map.h					\
	e-multi-config-dialog.h			\
	e-search-bar.h				\
	e-task-bar.h				\
	e-task-widget.h				\
	e-send-options.h			\
	e-url-entry.h				\
	e-canvas-background.h			\
	e-canvas-utils.h			\
	e-canvas-vbox.h				\
	e-canvas.h				\
	e-cursors.h				\
	e-gui-utils.h				\
	e-hsv-utils.h				\
	e-popup-menu.h				\
	e-printable.h				\
	e-reflow-model.h			\
	e-reflow.h				\
	e-selection-model-array.h		\
	e-selection-model-simple.h		\
	e-selection-model.h			\
	e-unicode.h				\
	e-colors.h				\
	gal-combo-box.h				\
	gal-combo-text.h

libemiscwidgets_la_SOURCES =			\
	$(widgetsinclude_HEADERS)		\
	$(pilot_sources)			\
	e-activity-handler.c			\
	e-calendar.c				\
	e-attachment.c				\
	e-attachment-bar.c			\
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
	e-expander.c				\
	e-image-chooser.c			\
	e-info-label.c				\
	e-map.c					\
	e-multi-config-dialog.c			\
	e-search-bar.c				\
	e-task-bar.c				\
	e-task-widget.c				\
	e-send-options.c			\
	e-url-entry.c				\
	e-canvas-background.c			\
	e-canvas-utils.c			\
	e-canvas-vbox.c				\
	e-canvas.c				\
	e-cursors.c				\
	e-gui-utils.c				\
	e-hsv-utils.c				\
	e-popup-menu.c				\
	e-printable.c				\
	e-reflow-model.c			\
	e-reflow.c				\
	e-selection-model-array.c		\
	e-selection-model-simple.c		\
	e-selection-model.c			\
	e-unicode.c				\
	e-colors.c				\
	gal-combo-box.c				\
	gal-combo-text.c


libemiscwidgets_la_LDFLAGS = $(NO_UNDEFINED)

libemiscwidgets_la_LIBADD = $(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/widgets/table/libetable.la			\
	$(top_builddir)/widgets/text/libetext.la			\
	$(top_builddir)/a11y/widgets/libevolution-widgets-a11y.la	\
	$(top_builddir)/a11y/libevolution-a11y.la			\
	$(CAMEL_LIBS)							\
	$(EXTRA_GNOME_LIBS)						\
	$(ICONV_LIBS)

libefilterbar_la_SOURCES =	\
	e-filter-bar.c		\
	e-filter-bar.h

libefilterbar_la_LDFLAGS = $(NO_UNDEFINED)

libefilterbar_la_LIBADD =			\
	$(WIN32_BOOTSTRAP_LIBS)			\
	libemiscwidgets.la			\
	$(E_WIDGETS_LIBS)

noinst_PROGRAMS = 			\
	test-calendar			\
	test-dateedit			\
	test-dropdown-button		\
	test-multi-config-dialog 	\
	test-info-label

# test-calendar

test_calendar_SOURCES = 	\
	test-calendar.c

test_calendar_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)

# test-dateedit

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)

# test-dropdown-button

test_dropdown_button_SOURCES = 	\
	test-dropdown-button.c

test_dropdown_button_LDADD =			\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la  	\
	$(E_WIDGETS_LIBS)

# test-multi-config-dialog

test_multi_config_dialog_SOURCES = 	\
	test-multi-config-dialog.c

test_multi_config_dialog_LDADD = 		\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)

# test-info-label

test_info_label_SOURCES = 	\
	test-info-label.c

test_info_label_LDADD = 			\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(E_WIDGETS_LIBS)


EXTRA_DIST =			\
	$(glade_DATA)		\
	e-pilot-settings.h	\
	e-pilot-settings.c	\
	e-util-marshal.list	\
	ChangeLog.pre-1-4

dist-hook:
	-mkdir $(distdir)/pixmaps
	cp $(srcdir)/pixmaps/*.xpm $(distdir)/pixmaps

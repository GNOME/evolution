if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libfilter.la
endif

AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/a11y/widgets					\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_MAIL_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)

privsolib_LTLIBRARIES =		\
	libemiscwidgets.la	\
	libefilterbar.la

widgetsincludedir = $(privincludedir)/misc
filterbarincludedir = $(privincludedir)/misc

if ENABLE_PILOT_CONDUITS
pilot_sources = e-pilot-settings.c
pilot_headers = e-pilot-settings.h
else
pilot_sources =
pilot_headers =
endif

glade_DATA = e-send-options.glade

widgetsinclude_HEADERS =			\
	$(pilot_headers)			\
	e-account-combo-box.h			\
	e-activity-handler.h			\
	e-attachment.h				\
	e-attachment-button.h			\
	e-attachment-dialog.h			\
	e-attachment-handler.h			\
	e-attachment-handler-image.h		\
	e-attachment-handler-sendto.h		\
	e-attachment-icon-view.h		\
	e-attachment-paned.h			\
	e-attachment-store.h			\
	e-attachment-tree-view.h		\
	e-attachment-view.h			\
	e-spinner.c				\
	e-spinner.h				\
	e-calendar.h				\
	e-calendar-item.h			\
	e-cell-date-edit.h			\
	e-cell-percent.h			\
	e-cell-renderer-combo.h			\
	e-charset-picker.h			\
	e-combo-cell-editable.h			\
	e-config-page.h				\
	e-combo-button.h			\
	e-dateedit.h				\
	e-dropdown-button.h			\
	e-icon-entry.h				\
	e-image-chooser.h			\
	e-info-label.h				\
	e-map.h					\
	e-multi-config-dialog.h			\
	e-online-button.h			\
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
	e-signature-combo-box.h			\
	e-unicode.h				\
	e-colors.h

libemiscwidgets_la_SOURCES =			\
	$(widgetsinclude_HEADERS)		\
	$(pilot_sources)			\
	e-account-combo-box.c			\
	e-activity-handler.c			\
	e-calendar.c				\
	e-attachment.c				\
	e-attachment-button.c			\
	e-attachment-dialog.c			\
	e-attachment-handler.c			\
	e-attachment-handler-image.c		\
	e-attachment-handler-sendto.c		\
	e-attachment-icon-view.c		\
	e-attachment-paned.c			\
	e-attachment-store.c			\
	e-attachment-tree-view.c		\
	e-attachment-view.c			\
	e-calendar-item.c			\
	e-cell-date-edit.c			\
	e-cell-percent.c			\
	e-cell-renderer-combo.c			\
	e-charset-picker.c			\
	e-combo-cell-editable.c			\
	e-config-page.c				\
	e-combo-button.c			\
	e-dateedit.c				\
	e-dropdown-button.c			\
	e-icon-entry.c				\
	e-image-chooser.c			\
	e-info-label.c				\
	e-map.c					\
	e-multi-config-dialog.c			\
	e-online-button.c			\
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
	e-signature-combo-box.c			\
	e-unicode.c				\
	e-colors.c


libemiscwidgets_la_LDFLAGS = $(NO_UNDEFINED)

libemiscwidgets_la_LIBADD = $(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/widgets/table/libetable.la			\
	$(top_builddir)/widgets/text/libetext.la			\
	$(top_builddir)/a11y/widgets/libevolution-widgets-a11y.la	\
	$(top_builddir)/a11y/libevolution-a11y.la			\
	$(EVOLUTION_MAIL_LIBS)						\
	$(GNOME_PLATFORM_LIBS)						\
	$(EVOLUTON_MAIL_LIBS)						\
	$(ICONV_LIBS)

filterbarinclude_HEADERS = e-filter-bar.h

libefilterbar_la_SOURCES =	\
	e-filter-bar.c		\
	$(filterbarinclude_HEADERS)

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
	ChangeLog.pre-1-4

dist-hook:
	-mkdir $(distdir)/pixmaps
	cp $(srcdir)/pixmaps/*.xpm $(distdir)/pixmaps

-include $(top_srcdir)/git.mk

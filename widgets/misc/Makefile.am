privsolib_LTLIBRARIES = libemiscwidgets.la

widgetsincludedir = $(privincludedir)/misc

ui_DATA = e-send-options.ui

widgetsinclude_HEADERS =			\
	e-account-combo-box.h			\
	e-account-manager.h			\
	e-account-tree-view.h			\
	e-action-combo-box.h			\
	e-activity-proxy.h			\
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
	e-buffer-tagger.h			\
	e-calendar.h				\
	e-calendar-item.h			\
	e-canvas.h				\
	e-canvas-background.h			\
	e-canvas-utils.h			\
	e-canvas-vbox.h				\
	e-cell-renderer-combo.h			\
	e-charset-combo-box.h			\
	e-combo-cell-editable.h			\
	e-dateedit.h				\
	e-focus-tracker.h			\
	e-hinted-entry.h			\
	e-hsv-utils.h				\
	e-image-chooser.h			\
	e-import-assistant.h			\
	e-map.h					\
	e-menu-tool-action.h			\
	e-menu-tool-button.h			\
	e-online-button.h			\
	e-paned.h				\
	e-popup-action.h			\
	e-popup-menu.h				\
	e-preferences-window.h			\
	e-preview-pane.h			\
	e-printable.h				\
	e-search-bar.h				\
	e-searching-tokenizer.h			\
	e-selectable.h				\
	e-selection-model.h			\
	e-selection-model-array.h		\
	e-selection-model-simple.h		\
	e-send-options.h			\
	e-signature-combo-box.h			\
	e-signature-editor.h			\
	e-signature-manager.h			\
	e-signature-preview.h			\
	e-signature-script-dialog.h		\
	e-signature-tree-view.h			\
	e-url-entry.h				\
	e-web-view.h				\
	e-web-view-preview.h			\
	ea-calendar-cell.h			\
	ea-calendar-item.h			\
	ea-cell-table.h				\
	ea-widgets.h

libemiscwidgets_la_CPPFLAGS =						\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/filter						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"				\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_MAIL_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)

libemiscwidgets_la_SOURCES =			\
	$(widgetsinclude_HEADERS)		\
	e-account-combo-box.c			\
	e-account-manager.c			\
	e-account-tree-view.c			\
	e-action-combo-box.c			\
	e-activity-proxy.c			\
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
	e-buffer-tagger.c			\
	e-calendar.c				\
	e-calendar-item.c			\
	e-canvas.c				\
	e-canvas-background.c			\
	e-canvas-utils.c			\
	e-canvas-vbox.c				\
	e-cell-renderer-combo.c			\
	e-charset-combo-box.c			\
	e-combo-cell-editable.c			\
	e-dateedit.c				\
	e-focus-tracker.c			\
	e-hinted-entry.c			\
	e-hsv-utils.c				\
	e-image-chooser.c			\
	e-import-assistant.c			\
	e-map.c					\
	e-menu-tool-action.c			\
	e-menu-tool-button.c			\
	e-online-button.c			\
	e-paned.c				\
	e-popup-action.c			\
	e-popup-menu.c				\
	e-preferences-window.c			\
	e-preview-pane.c			\
	e-printable.c				\
	e-search-bar.c				\
	e-searching-tokenizer.c			\
	e-selectable.c				\
	e-selection-model.c			\
	e-selection-model-array.c		\
	e-selection-model-simple.c		\
	e-send-options.c			\
	e-signature-combo-box.c			\
	e-signature-editor.c			\
	e-signature-manager.c			\
	e-signature-preview.c			\
	e-signature-script-dialog.c		\
	e-signature-tree-view.c			\
	e-url-entry.c				\
	e-web-view.c				\
	e-web-view-preview.c			\
	ea-calendar-cell.c			\
	ea-calendar-item.c			\
	ea-cell-table.c				\
	ea-widgets.c

libemiscwidgets_la_LDFLAGS = $(NO_UNDEFINED)

libemiscwidgets_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/a11y/libevolution-a11y.la		\
	$(top_builddir)/libgnomecanvas/libgnomecanvas.la	\
	$(top_builddir)/libart_lgpl/libart_lgpl.la		\
	$(EVOLUTION_MAIL_LIBS)					\
	$(GNOME_PLATFORM_LIBS)					\
	$(MATH_LIB)						\
	$(ICONV_LIBS)

noinst_PROGRAMS = 			\
	test-calendar			\
	test-dateedit			\
	test-preferences-window	

test_widgets_misc_CPPFLAGS=						\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/filter						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_MAIL_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)

# test-calendar

test_calendar_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_calendar_SOURCES = 	\
	test-calendar.c

test_calendar_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(GNOME_PLATFORM_LIBS)			\
	$(E_WIDGETS_LIBS)

# test-dateedit

test_dateedit_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(GNOME_PLATFORM_LIBS)			\
	$(E_WIDGETS_LIBS)

# test-preferences-window

test_preferences_window_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_preferences_window_SOURCES =		\
	test-preferences-window.c

test_preferences_window_LDADD = 		\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(GNOME_PLATFORM_LIBS)			\
	$(E_WIDGETS_LIBS)

EXTRA_DIST = $(ui_DATA)

-include $(top_srcdir)/git.mk

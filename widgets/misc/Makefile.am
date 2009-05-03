if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libfilter.la
endif

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/a11y/widgets					\
	-I$(top_srcdir)/filter						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"				\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_MAIL_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(ICONV_CFLAGS)


privsolib_LTLIBRARIES =		\
	libemiscwidgets.la

widgetsincludedir = $(privincludedir)/misc

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
	e-action-combo-box.h			\
	e-activity.h				\
	e-activity-proxy.h			\
	e-alert-activity.h			\
	e-attachment.h				\
	e-attachment-button.h			\
	e-attachment-dialog.h			\
	e-attachment-handler.h			\
	e-attachment-handler-image.h		\
	e-attachment-icon-view.h		\
	e-attachment-paned.h			\
	e-attachment-store.h			\
	e-attachment-tree-view.h		\
	e-attachment-view.h			\
	e-calendar.h				\
	e-calendar-item.h			\
	e-camel-activity.h			\
	e-canvas.h				\
	e-canvas-background.h			\
	e-canvas-utils.h			\
	e-canvas-vbox.h				\
	e-cell-date-edit.h			\
	e-cell-percent.h			\
	e-cell-renderer-combo.h			\
	e-charset-picker.h			\
	e-colors.h				\
	e-combo-cell-editable.h			\
	e-cursors.h				\
	e-dateedit.h				\
	e-file-activity.h			\
	e-gui-utils.h				\
	e-hsv-utils.h				\
	e-icon-entry.h				\
	e-image-chooser.h			\
	e-map.h					\
	e-menu-tool-button.h			\
	e-online-button.h			\
	e-popup-action.h			\
	e-popup-menu.h				\
	e-preferences-window.h			\
	e-printable.h				\
	e-reflow.h				\
	e-reflow-model.h			\
	e-search-bar.h				\
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
	e-spinner.c				\
	e-spinner.h				\
	e-timeout-activity.h			\
	e-unicode.h				\
	e-url-entry.h

libemiscwidgets_la_SOURCES =			\
	$(widgetsinclude_HEADERS)		\
	$(pilot_sources)			\
	e-account-combo-box.c			\
	e-action-combo-box.c			\
	e-activity.c				\
	e-activity-proxy.c			\
	e-alert-activity.c			\
	e-attachment.c				\
	e-attachment-button.c			\
	e-attachment-dialog.c			\
	e-attachment-handler.c			\
	e-attachment-handler-image.c		\
	e-attachment-icon-view.c		\
	e-attachment-paned.c			\
	e-attachment-store.c			\
	e-attachment-tree-view.c		\
	e-attachment-view.c			\
	e-calendar.c				\
	e-calendar-item.c			\
	e-camel-activity.c			\
	e-canvas.c				\
	e-canvas-background.c			\
	e-canvas-utils.c			\
	e-canvas-vbox.c				\
	e-cell-date-edit.c			\
	e-cell-percent.c			\
	e-cell-renderer-combo.c			\
	e-charset-picker.c			\
	e-colors.c				\
	e-combo-cell-editable.c			\
	e-cursors.c				\
	e-dateedit.c				\
	e-file-activity.c			\
	e-gui-utils.c				\
	e-hsv-utils.c				\
	e-icon-entry.c				\
	e-image-chooser.c			\
	e-map.c					\
	e-menu-tool-button.c			\
	e-online-button.c			\
	e-popup-action.c			\
	e-popup-menu.c				\
	e-preferences-window.c			\
	e-printable.c				\
	e-reflow-model.c			\
	e-reflow.c				\
	e-search-bar.c				\
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
	e-timeout-activity.c			\
	e-unicode.c				\
	e-url-entry.c

libemiscwidgets_la_LDFLAGS = $(NO_UNDEFINED)

libemiscwidgets_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/filter/libfilter.la				\
	$(top_builddir)/widgets/table/libetable.la			\
	$(top_builddir)/widgets/text/libetext.la			\
	$(top_builddir)/a11y/widgets/libevolution-widgets-a11y.la	\
	$(top_builddir)/a11y/libevolution-a11y.la			\
	$(EVOLUTION_MAIL_LIBS)						\
	$(GNOME_PLATFORM_LIBS)						\
	$(ICONV_LIBS)

noinst_PROGRAMS = 			\
	test-calendar			\
	test-dateedit			\
	test-preferences-window	

# test-calendar

test_calendar_SOURCES = 	\
	test-calendar.c

test_calendar_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(E_WIDGETS_LIBS)

# test-dateedit

test_dateedit_SOURCES = 	\
	test-dateedit.c

test_dateedit_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(E_WIDGETS_LIBS)

# test-preferences-window

test_preferences_window_SOURCES =		\
	test-preferences-window.c

test_preferences_window_LDADD = 		\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
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

privsolib_LTLIBRARIES = libemiscwidgets.la

widgetsincludedir = $(privincludedir)/misc

ui_DATA = e-send-options.ui

widgetsinclude_HEADERS =			\
	e-action-combo-box.h			\
	e-activity-bar.h			\
	e-activity-proxy.h			\
	e-alarm-selector.h			\
	e-alert-bar.h				\
	e-attachment.h				\
	e-attachment-bar.h			\
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
	e-auth-combo-box.h			\
	e-autocomplete-selector.h		\
	e-book-source-config.h			\
	e-buffer-tagger.h			\
	e-cal-source-config.h			\
	e-calendar.h				\
	e-calendar-item.h			\
	e-canvas.h				\
	e-canvas-background.h			\
	e-canvas-utils.h			\
	e-canvas-vbox.h				\
	e-charset-combo-box.h			\
	e-contact-map.h				\
	e-contact-map-window.h			\
	e-contact-marker.h			\
	e-dateedit.h				\
	e-focus-tracker.h			\
	e-image-chooser.h			\
	e-import-assistant.h			\
	e-interval-chooser.h			\
	e-mail-identity-combo-box.h		\
	e-mail-signature-combo-box.h		\
	e-mail-signature-editor.h		\
	e-mail-signature-manager.h		\
	e-mail-signature-preview.h		\
	e-mail-signature-script-dialog.h	\
	e-mail-signature-tree-view.h		\
	e-map.h					\
	e-menu-tool-action.h			\
	e-menu-tool-button.h			\
	e-online-button.h			\
	e-paned.h				\
	e-picture-gallery.h			\
	e-popup-action.h			\
	e-port-entry.h				\
	e-preferences-window.h			\
	e-preview-pane.h			\
	e-printable.h				\
	e-search-bar.h				\
	e-selectable.h				\
	e-selection-model.h			\
	e-selection-model-array.h		\
	e-selection-model-simple.h		\
	e-send-options.h			\
	e-source-config.h			\
	e-source-config-backend.h		\
	e-source-config-dialog.h		\
	e-spell-entry.h				\
	e-url-entry.h				\
	e-web-view.h				\
	e-web-view-gtkhtml.h			\
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
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"				\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"    		\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(GTKHTML_CFLAGS)						\
	$(CHAMPLAIN_CFLAGS)						\
	$(GEO_CFLAGS)							\
	$(CLUTTER_CFLAGS)

libemiscwidgets_la_SOURCES =			\
	$(widgetsinclude_HEADERS)		\
	e-action-combo-box.c			\
	e-activity-bar.c			\
	e-activity-proxy.c			\
	e-alarm-selector.c			\
	e-alert-bar.c				\
	e-attachment.c				\
	e-attachment-bar.c			\
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
	e-auth-combo-box.c			\
	e-autocomplete-selector.c		\
	e-book-source-config.c			\
	e-buffer-tagger.c			\
	e-cal-source-config.c			\
	e-calendar.c				\
	e-calendar-item.c			\
	e-canvas.c				\
	e-canvas-background.c			\
	e-canvas-utils.c			\
	e-canvas-vbox.c				\
	e-charset-combo-box.c			\
	e-contact-map.c				\
	e-contact-map-window.c			\
	e-contact-marker.c			\
	e-dateedit.c				\
	e-focus-tracker.c			\
	e-image-chooser.c			\
	e-import-assistant.c			\
	e-interval-chooser.c			\
	e-mail-identity-combo-box.c		\
	e-mail-signature-combo-box.c		\
	e-mail-signature-editor.c		\
	e-mail-signature-manager.c		\
	e-mail-signature-preview.c		\
	e-mail-signature-script-dialog.c	\
	e-mail-signature-tree-view.c		\
	e-map.c					\
	e-menu-tool-action.c			\
	e-menu-tool-button.c			\
	e-online-button.c			\
	e-paned.c				\
	e-picture-gallery.c			\
	e-popup-action.c			\
	e-port-entry.c				\
	e-preferences-window.c			\
	e-preview-pane.c			\
	e-printable.c				\
	e-search-bar.c				\
	e-selectable.c				\
	e-selection-model.c			\
	e-selection-model-array.c		\
	e-selection-model-simple.c		\
	e-send-options.c			\
	e-source-config.c			\
	e-source-config-backend.c		\
	e-source-config-dialog.c		\
	e-spell-entry.c				\
	e-url-entry.c				\
	e-web-view.c				\
	e-web-view-gtkhtml.c			\
	e-web-view-preview.c			\
	ea-calendar-cell.c			\
	ea-calendar-item.c			\
	ea-cell-table.c				\
	ea-widgets.c

libemiscwidgets_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libemiscwidgets_la_LIBADD =					\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/a11y/libevolution-a11y.la		\
	$(top_builddir)/libgnomecanvas/libgnomecanvas.la	\
	$(top_builddir)/libemail-utils/libemail-utils.la	\
	$(top_builddir)/libevolution-utils/libevolution-utils.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(MATH_LIB)						\
	$(ICONV_LIBS)						\
	$(CHAMPLAIN_LIBS)					\
	$(GEO_LIBS)						\
	$(CLUTTER_LIBS)						\
	$(GTKHTML_LIBS)

error_DATA = widgets.error
errordir = $(privdatadir)/errors
# provides error rules too
@EVO_PLUGIN_RULE@

noinst_PROGRAMS = 			\
	test-calendar			\
	test-dateedit			\
	test-mail-signatures		\
	test-preferences-window		\
	test-source-config

test_widgets_misc_CPPFLAGS=						\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/filter						\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"			\
	-DG_LOG_DOMAIN=__FILE__						\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)

# test-calendar

test_calendar_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_calendar_SOURCES = test-calendar.c

test_calendar_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)

# test-dateedit

test_dateedit_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_dateedit_SOURCES = test-dateedit.c

test_dateedit_LDADD = 				\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)

# test-mail-signatures

test_mail_signatures_CPPFLAGS =			\
	$(test_widgets_misc_CPPFLAGS)		\
	$(GTKHTML_CFLAGS)

test_mail_signatures_SOURCES = test-mail-signatures.c

test_mail_signatures_LDADD =			\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(top_builddir)/libevolution-utils/libevolution-utils.la \
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)			\
	$(GTKHTML_LIBS)

# test-preferences-window

test_preferences_window_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_preferences_window_SOURCES = test-preferences-window.c

test_preferences_window_LDADD = 		\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)

# test-source-config

test_source_config_CPPFLAGS = $(test_widgets_misc_CPPFLAGS)

test_source_config_SOURCES = test-source-config.c

test_source_config_LDADD = 			\
	libemiscwidgets.la			\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/filter/libfilter.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)

BUILT_SOURCES = $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(ui_DATA) widgets.error.xml

-include $(top_srcdir)/git.mk

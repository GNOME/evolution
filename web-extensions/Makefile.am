webextensions_LTLIBRARIES = libewebextension.la libehtmleditorwebextension.la libedomutils.la

libedomutils_la_SOURCES =				\
	e-dom-utils.h					\
	e-dom-utils.c

libedomutils_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DLIBEUTIL_COMPILATION				\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

libedomutils_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)

libewebextension_la_SOURCES =				\
	e-web-extension.h				\
	e-web-extension-names.h				\
	e-dom-utils.h					\
	e-web-extension.c				\
	e-web-extension-main.c				\
	e-dom-utils.c

libewebextension_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DLIBEUTIL_COMPILATION				\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libewebextension_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libewebextension_la_LDFLAGS =				\
	-module -avoid-version -no-undefined

libehtmleditorwebextension_la_SOURCES =			\
	e-composer-private-dom-functions.h		\
	e-dom-utils.h					\
	e-html-editor-actions-dom-functions.h		\
	e-html-editor-cell-dialog-dom-functions.h	\
	e-html-editor-history-event.h			\
	e-html-editor-hrule-dialog-dom-functions.h	\
	e-html-editor-image-dialog-dom-functions.h	\
	e-html-editor-link-dialog-dom-functions.h	\
	e-html-editor-page-dialog-dom-functions.h	\
	e-html-editor-selection-dom-functions.h		\
	e-html-editor-spell-check-dialog-dom-functions.h\
	e-html-editor-table-dialog-dom-functions.h	\
	e-html-editor-undo-redo-manager.h		\
	e-html-editor-view-dom-functions.h		\
	e-msg-composer-dom-functions.h			\
	e-composer-private-dom-functions.c		\
	e-dom-utils.c					\
	e-html-editor-actions-dom-functions.c		\
	e-html-editor-cell-dialog-dom-functions.c	\
	e-html-editor-hrule-dialog-dom-functions.c	\
	e-html-editor-image-dialog-dom-functions.c	\
	e-html-editor-link-dialog-dom-functions.c	\
	e-html-editor-page-dialog-dom-functions.c	\
	e-html-editor-selection-dom-functions.c		\
	e-html-editor-spell-check-dialog-dom-functions.c\
	e-html-editor-table-dialog-dom-functions.c	\
	e-html-editor-undo-redo-manager.c		\
	e-html-editor-view-dom-functions.c		\
	e-msg-composer-dom-functions.c			\
	e-html-editor-web-extension.c			\
	e-html-editor-web-extension.h			\
	e-html-editor-web-extension-names.h		\
	e-html-editor-web-extension-main.c

libehtmleditorwebextension_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DLIBEUTIL_COMPILATION				\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libehtmleditorwebextension_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libehtmleditorwebextension_la_LDFLAGS =			\
	-module -avoid-version -no-undefined

webextensionswebkiteditor_LTLIBRARIES = libewebkiteditorwebextension.la

libewebkiteditorwebextension_la_SOURCES = \
	e-composer-dom-functions.c	\
	e-composer-dom-functions.h	\
	e-dialogs-dom-functions.c	\
	e-dialogs-dom-functions.h	\
	e-editor-dom-functions.c	\
	e-editor-dom-functions.h	\
	e-editor-page.c			\
	e-editor-page.h			\
	e-editor-undo-redo-manager.c	\
	e-editor-undo-redo-manager.h	\
	e-editor-web-extension.c	\
	e-editor-web-extension.h	\
	e-editor-web-extension-main.c	\
	e-editor-web-extension-names.h	\
	$(NULL)

libewebkiteditorwebextension_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(WEB_EXTENSIONS_CFLAGS)

libewebkiteditorwebextension_la_LIBADD =		\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/web-extensions/libedomutils.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(WEB_EXTENSIONS_LIBS)

libewebkiteditorwebextension_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

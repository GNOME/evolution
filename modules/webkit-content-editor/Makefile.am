SUBDIRS = . web-extension

module_LTLIBRARIES = module-webkit-content-editor.la

module_webkit_content_editor_la_CPPFLAGS = 		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_WEB_EXTENSIONS_COMPOSER_DIR=\""$(webextensionscomposerdir)"\" \
	-DG_LOG_DOMAIN=\"webkit-content-editor\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

module_webkit_content_editor_la_SOURCES = \
	evolution-module-webkit-content-editor.c	\
	e-webkit-editor-extension.c			\
	e-webkit-editor-extension.h			\
	e-webkit-content-editor.c			\
	e-webkit-content-editor.h			\
	e-webkit-content-editor-find-controller.c	\
	e-webkit-content-editor-find-controller.h

module_webkit_content_editor_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

module_webkit_content_editor_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

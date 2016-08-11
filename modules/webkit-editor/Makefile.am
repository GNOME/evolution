SUBDIRS = . web-extension

module_LTLIBRARIES = module-webkit-editor.la

module_webkit_editor_la_CPPFLAGS = 		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-DEVOLUTION_WEB_EXTENSIONS_WEBKIT_EDITOR_DIR=\""$(webextensionswebkiteditordir)"\" \
	-DG_LOG_DOMAIN=\"webkit-editor\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

module_webkit_editor_la_SOURCES = \
	evolution-module-webkit-editor.c		\
	e-webkit-editor-extension.c			\
	e-webkit-editor-extension.h			\
	e-webkit-editor.c				\
	e-webkit-editor.h

module_webkit_editor_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

module_webkit_editor_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk

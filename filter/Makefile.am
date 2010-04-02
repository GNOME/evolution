ui_DATA = filter.ui

privsolib_LTLIBRARIES =	libfilter.la

libfilter_la_CPPFLAGS = 			\
	$(AM_CPPFLAGS)				\
	-I $(top_srcdir)			\
	-I $(top_srcdir)/e-util			\
	-DEVOLUTION_UIDIR=\"$(uidir)\"		\
	-DG_LOG_DOMAIN=\"filter\"		\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(LIBFILTER_CFLAGS)

filterincludedir = $(privincludedir)/filter

filterinclude_HEADERS = 			\
	e-filter-code.h				\
	e-filter-color.h			\
	e-filter-datespec.h			\
	e-filter-element.h			\
	e-filter-file.h				\
	e-filter-input.h			\
	e-filter-int.h				\
	e-filter-option.h			\
	e-filter-part.h				\
	e-filter-rule.h				\
	e-rule-context.h			\
	e-rule-editor.h

libfilter_la_SOURCES =				\
	e-filter-code.c				\
	e-filter-code.h				\
	e-filter-color.c			\
	e-filter-color.h			\
	e-filter-datespec.c			\
	e-filter-datespec.h			\
	e-filter-element.c			\
	e-filter-element.h			\
	e-filter-file.c				\
	e-filter-file.h				\
	e-filter-input.c			\
	e-filter-input.h			\
	e-filter-int.c				\
	e-filter-int.h				\
	e-filter-option.c			\
	e-filter-option.h			\
	e-filter-part.c				\
	e-filter-part.h				\
	e-filter-rule.c				\
	e-filter-rule.h				\
	e-rule-context.c			\
	e-rule-context.h			\
	e-rule-editor.c				\
	e-rule-editor.h

libfilter_la_LDFLAGS = $(NO_UNDEFINED)

libfilter_la_LIBADD = 				\
	$(top_builddir)/e-util/libeutil.la	\
	$(LIBFILTER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)			\
	$(REGEX_LIBS)

EXTRA_DIST =					\
	$(ui_DATA)				\
	filter.error.xml

# basic rules.
error_DATA = filter.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

BUILT_SOURCES = $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

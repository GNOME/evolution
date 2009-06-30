
glade_DATA = filter.glade

AM_CPPFLAGS = 					\
	-I $(top_srcdir)			\
	-I $(top_srcdir)/e-util			\
	-I $(top_srcdir)/widgets/misc		\
	-DEVOLUTION_GLADEDIR=\"$(gladedir)\"	\
	-DG_LOG_DOMAIN=\"filter\"		\
	$(LIBFILTER_CFLAGS)

privsolib_LTLIBRARIES =				\
	libfilter.la

filterincludedir = $(privincludedir)/filter

filterinclude_HEADERS = 			\
	filter-code.h				\
	filter-colour.h				\
	filter-datespec.h			\
	filter-element.h			\
	filter-file.h				\
	filter-input.h				\
	filter-int.h				\
	filter-option.h				\
	filter-part.h				\
	filter-rule.h				\
	rule-context.h				\
	rule-editor.h

libfilter_la_SOURCES =				\
	filter-code.c				\
	filter-code.h				\
	filter-colour.c				\
	filter-colour.h				\
	filter-datespec.c			\
	filter-datespec.h			\
	filter-element.c			\
	filter-element.h			\
	filter-file.c				\
	filter-file.h				\
	filter-input.c				\
	filter-input.h				\
	filter-int.c				\
	filter-int.h				\
	filter-option.c				\
	filter-option.h				\
	filter-part.c				\
	filter-part.h				\
	filter-rule.c				\
	filter-rule.h				\
	rule-context.c				\
	rule-context.h				\
	rule-editor.c				\
	rule-editor.h

libfilter_la_LDFLAGS = $(NO_UNDEFINED)

libfilter_la_LIBADD = 				\
	$(top_builddir)/e-util/libeutil.la	\
	$(LIBFILTER_LIBS)			\
	$(REGEX_LIBS)

EXTRA_DIST =					\
	$(glade_DATA)				\
	filter.error.xml			\
	ChangeLog.pre-1-4

# basic rules.
error_DATA = filter.error
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

BUILT_SOURCES = $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

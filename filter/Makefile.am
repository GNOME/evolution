
glade_DATA = filter.glade

INCLUDES = 					\
	-I $(top_srcdir)			\
	-I $(top_srcdir)/e-util			\
	-I $(top_srcdir)/widgets/misc		\
	-DFILTER_GLADEDIR=\"$(gladedir)\"	\
	-DG_LOG_DOMAIN=\"filter\"		\
	$(LIBFILTER_CFLAGS)

noinst_LTLIBRARIES =				\
	libfilter.la

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
	filter-label.c				\
	filter-label.h				\
	filter-marshal.c			\
	filter-marshal.h			\
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

EXTRA_DIST =					\
	$(glade_DATA)				\
	$(error_DATA)				\
	$(error_i18n)				\
	ChangeLog.pre-1-4			\
	filter-marshal.list

# basic rules.
error_DATA = filter-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

MARSHAL_GENERATED = filter-marshal.c filter-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(MARSHAL_GENERATED) $(error_i18n)
CLEANFILES = $(BUILT_SOURCES)

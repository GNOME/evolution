
glade_DATA = filter.glade

INCLUDES = 					\
	-I $(top_srcdir)			\
	-I $(top_srcdir)/camel			\
	-I $(top_srcdir)/e-util			\
	-I $(top_srcdir)/shell			\
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
	filter-context.c			\
	filter-context.h			\
	filter-datespec.c			\
	filter-datespec.h			\
	filter-editor.c				\
	filter-editor.h				\
	filter-element.c			\
	filter-element.h			\
	filter-file.c				\
	filter-file.h				\
	filter-filter.c				\
	filter-filter.h				\
	filter-folder.c				\
	filter-folder.h				\
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
	filter-source.h				\
	filter-source.c				\
	rule-context.c				\
	rule-context.h				\
	rule-editor.c				\
	rule-editor.h				\
	score-context.c				\
	score-context.h				\
	score-editor.c				\
	score-editor.h				\
	score-rule.c				\
	score-rule.h				\
	vfolder-context.c			\
	vfolder-context.h			\
	vfolder-editor.c			\
	vfolder-editor.h			\
	vfolder-rule.c				\
	vfolder-rule.h

libfilter_la_DEPENDENCIES = libfilter-i18n.h

EXTRA_DIST =					\
	$(glade_DATA)				\
	$(error_in_files)			\
	ChangeLog.pre-1-4			\
	filter-marshal.list			\
	filtertypes.xml				\
	libfilter-i18n.h			\
	vfoldertypes.xml			\
	searchtypes.xml

# basic rules.
filterdir = $(privdatadir)
filter_DATA = filtertypes.xml vfoldertypes.xml searchtypes.xml

error_in_files = filter-errors.xml.in
error_DATA = $(error_in_files:.xml.in=.xml)
errordir = $(privdatadir)/errors
@INTLTOOL_XML_RULE@

libfilter-i18n.h: filtertypes.xml vfoldertypes.xml
	echo "/* Automatically generated. Do not edit. */" > $@;	      \
	cat $(srcdir)/filtertypes.xml $(srcdir)/vfoldertypes.xml |	      \
		sed -n -e 's:.*<title>\(.*\)</title>:char *s = N_("\1");:p' | \
		sort -u >> $@

MARSHAL_GENERATED = filter-marshal.c filter-marshal.h
@EVO_MARSHAL_RULE@

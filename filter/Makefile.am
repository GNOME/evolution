
gladedir = $(datadir)/evolution/glade
glade_DATA = filter.glade

INCLUDES = 					\
	-I $(top_srcdir)			\
	-I $(top_srcdir)/libibex		\
	-I $(top_srcdir)/camel			\
	-I $(top_srcdir)/e-util			\
	-I $(top_srcdir)/shell			\
	-I $(top_srcdir)/camel/providers/mbox	\
	-I $(top_builddir)/shell		\
	-I $(GNOME_INCLUDEDIR)			\
	$(GTKHTML_CFLAGS)			\
	$(UNICODE_CFLAGS)			\
	$(BONOBO_GNOME_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)			\
	-DFILTER_GLADEDIR=\"$(gladedir)\"	\
	-DG_LOG_DOMAIN=\"filter\"
##

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
	filter-filter.c				\
	filter-filter.h				\
	filter-folder.c				\
	filter-folder.h				\
	filter-input.c				\
	filter-input.h				\
	filter-option.c				\
	filter-option.h				\
	filter-part.c				\
	filter-part.h				\
	filter-rule.c				\
	filter-rule.h				\
	filter-score.c				\
	filter-score.h				\
	filter-system-flag.c			\
	filter-system-flag.h			\
	filter-url.c				\
	filter-url.h				\
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

EXTRA_DIST = filtertypes.xml vfoldertypes.xml \
	$(glade_DATA) libfilter-i18n.h

# basic rules.
filterdir = $(datadir)/evolution
filter_DATA = filtertypes.xml vfoldertypes.xml

libfilter-i18n.h: filtertypes.xml vfoldertypes.xml
	echo "/* Automatically generated. Do not edit. */" > $@;	      \
	cat $(srcdir)/filtertypes.xml $(srcdir)/vfoldertypes.xml |	      \
		sed -n -e 's:.*<title>\(.*\)</title>:char *s = N_("\1");:p' | \
		sort -u >> $@

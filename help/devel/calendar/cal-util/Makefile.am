## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=evolution-cal-util

# The top-level SGML file.
DOC_MAIN_SGML_FILE=evolution-cal-util.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(EVOLUTION_DIR)/calendar/cal-util

CFLAGS ="						\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/libical/src/libical		\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	"

LDFLAGS="								\
	$(BONOBO_VFS_GNOME_LIBS)					\
	$(top_builddir)/calendar/cal-util/.libs/libcal-util.a		\
	$(top_builddir)/libical/src/libical/libical.a			\
	$(top_builddir)/libversit/.libs/libversit.al			\
	"

HTML_DIR=$(datadir)/gnome/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =					\
	tmpl/cal-util.sgml			\
	tmpl/evolution-cal-util-unused.sgml

evolution_cal_util_docdir = $(HTML_DIR)
evolution_cal_util_doc_DATA =			\
	evolution-cal-util.hierarchy		\
	evolution-cal-util.types		\
	evolution-cal-util-decl.txt		\
	evolution-cal-util-sections.txt

EXTRA_DIST = 					\
	$(evolution_cal_util_doc_DATA)

sgml/evolution-cal-util-doc.bottom: $(tmpl_sources)
	$(MAKE) sgml

scan:
	-(cd $(srcdir) \
	&& env CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) \
		 gtkdoc-scanobj --module=$(DOC_MODULE) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="calobj.h icalendar-save.h icalendar.h" )

templates: scan
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

dist-hook:
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : sgml templates scan

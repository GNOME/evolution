# The name of the module.
DOC_MODULE=evolution-cal-util

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
	$(top_builddir)/libical/src/libical/.libs/libical.a		\
	$(top_builddir)/libversit/.libs/libversit.al			\
	"

DOC_DIR=$(datadir)/gnome/html

DOC_DIR_INSTALL_FILES =				\
	 evolution-cal-util.args		\
	 evolution-cal-util.hierarchy		\
	 evolution-cal-util.signals		\
	 evolution-cal-util.types		\
	 evolution-cal-util-decl.txt		\
	 evolution-cal-util-sections.txt

SOURCE_FILES =							\
	$(top_srcdir)/calendar/cal-util/cal-component.c		\
	$(top_srcdir)/calendar/cal-util/cal-component.h		\
	$(top_srcdir)/calendar/cal-util/cal-recur.c		\
	$(top_srcdir)/calendar/cal-util/cal-recur.h		\
	$(top_srcdir)/calendar/cal-util/cal-util.c		\
	$(top_srcdir)/calendar/cal-util/cal-util.h		\
	$(top_srcdir)/calendar/cal-util/timeutil.c		\
	$(top_srcdir)/calendar/cal-util/timeutil.h

IGNORED_HEADER_FILES =	\
	calobj.h

scan_generated =				\
	evolution-cal-util-decl.txt		\
	evolution-cal-util.args			\
	evolution-cal-util.hierarchy		\
	evolution-cal-util.signals		\
	evolution-cal-util.types

tmpl_dependencies =				\
	evolution-cal-util-decl.txt		\
	evolution-cal-util-sections.txt		\
	evolution-cal-util.args			\
	evolution-cal-util.hierarchy		\
	evolution-cal-util.signals

tmpl_sources =					\
	tmpl/cal-component.sgml			\
	tmpl/cal-recur.sgml			\
	tmpl/cal-util.sgml			\
	tmpl/evolution-cal-util-unused.sgml	\
	tmpl/timeutil.sgml

tmpl_generated =				\
	evolution-cal-util-unused.txt

sgml_dependencies =				\
	evolution-cal-util-decl.txt		\
	evolution-cal-util-sections.txt		\
	evolution-cal-util.args			\
	evolution-cal-util.hierarchy		\
	evolution-cal-util.signals		\
	tmpl/cal-component.sgml			\
	tmpl/cal-recur.sgml			\
	tmpl/cal-util.sgml			\
	tmpl/timeutil.sgml

sgml_generated = 				\
	sgml/cal-component.sgml			\
	sgml/cal-recur.sgml			\
	sgml/cal-util.sgml			\
	sgml/evolution-cal-util-doc.bottom	\
	sgml/evolution-cal-util-doc.top		\
	sgml/object_index.sgml			\
	sgml/timeutil.sgml			\
	sgml/tree_index.sgml

EXTRA_DIST = 					\
	 evolution-cal-util.args		\
	 evolution-cal-util.hierarchy		\
	 evolution-cal-util.signals		\
	 evolution-cal-util.types		\
	 evolution-cal-util-decl.txt		\
	 evolution-cal-util-sections.txt

all: $(sgml_generated)

if ENABLE_GTK_DOC
scan $(scan_generated): $(SOURCE_FILES)
	-(cd $(srcdir)									\
	  && env CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS)					\
		 gtkdoc-scanobj --module=$(DOC_MODULE)					\
	  && gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)		\
			 --ignore-headers="$(IGNORED_HEADER_FILES)" )

templates $(tmpl_sources) $(tmpl_generated): $(tmpl_dependencies)
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml $(sgml_generated): $(sgml_dependencies)
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)
else
scan $(scan_generated): $(SOURCE_FILES)

templates $(tmpl_sources) $(tmpl_generated): $(tmpl_dependencies)

sgml $(sgml_generated): $(sgml_dependencies)
endif

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DOC_DIR)
	for i in $(DOC_DIR_INSTALL_FILES); do		\
		$(INSTALL_DATA) $$i $(DOC_DIR);		\
	done

dist-hook:
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY: scan templates sgml

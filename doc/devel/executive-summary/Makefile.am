# The name of the module.
DOC_MODULE=evolution-services

# The directory containing the source code
DOC_SOURCE_DIR=$(EVOLUTION_DIR)/executive-summary/evolution-services

CFLAGS ="						\
	-I$(top_srcdir)/executive-summary/evolution-services	\
	-I$(top_srcdir)/executive-summary			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	"

LDFLAGS="								\
	$(BONOBO_VFS_GNOME_LIBS)					\
	$(top_builddir)/executive-summary/evolution-services/.libs/libevolution-services.a	\
	"

DOC_DIR=$(datadir)/gnome/html

DOC_DIR_INSTALL_FILES =					\
	evolution-services.args			\
	evolution-services.hierarchy		\
	evolution-services.signals		\
	evolution-services.types			\
	evolution-services-decl.txt		\
	evolution-services-sections.txt

SOURCE_FILES =							\
	$(top_srcdir)/executive-summary/evolution-services/executive-summary-component.c	\
	$(top_srcdir)/executive-summary/evolution-services/executive-summary-component-factory-client.c		\
	$(top_srcdir)/executive-summary/evolution-services/executive-summary-html-view.c

IGNORED_HEADER_FILES =		\
	Executive-Summary.h

scan_generated =						\
	evolution-services-decl.txt			\
	evolution-services.args				\
	evolution-services.hierarchy			\
	evolution-services.signals			\
	evolution-services.types

tmpl_dependencies =						\
	evolution-services-decl.txt			\
	evolution-services-sections.txt			\
	evolution-services.args				\
	evolution-services.hierarchy			\
	evolution-services.signals

tmpl_sources = 						\
	tmpl/evolution-services.sgml		\
	tmpl/evolution-services-unused.sgml

tmpl_generated =					\
	evolution-services-unused.txt

sgml_dependencies =					\
	evolution-services-decl.txt		\
	evolution-services-sections.txt		\
	evolution-services.args			\
	evolution-services.hierarchy		\
	evolution-services.signals		\
	tmpl/evolution-services.sgml

sgml_generated =						\
	sgml/evolution-services.sgml					\
	sgml/evolution-services-doc.bottom		\
	sgml/evolution-services-doc.top			\
	sgml/object_index.sgml					\
	sgml/tree_index.sgml

EXTRA_DIST =							\
	evolution-services.args				\
	evolution-services.hierarchy			\
	evolution-services.signals			\
	evolution-services.types				\
	evolution-services-decl.txt			\
	evolution-services-sections.txt

all: $(sgml_generated)

if ENABLE_GTK_DOC
scan $(scan_generated): $(SOURCE_FILES)
	-(cd $(srcdir)						\
	  && env CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS)		\
		 gtkdoc-scanobj --module=$(DOC_MODULE)		\
	  && gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)		\
			 --ignore-headers="$(IGNORED_HEADER_FILES)" )

templates $(tmpl_sources) $(tmpl_generated): $(tmpl_dependencies)
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml $(sgml_generated): $(sgml_dependencies)
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)
else
scan $(scan_generated): $(SOURCE_FILE)

templates $(tmpl_sources) $(tmpl_generated): $(tmpl_dependencies)

sgml $(sgml_generated): $(sgml_dependencies)
endif

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DOC_DIR)
	for i in $(DOC_DIR_INSTALL_FILES); do			\
		$(INSTALL_DATA) $(srcdir)/$$i $(DOC_DIR);	\
	done

dist-hook:
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY: scan templates sgml

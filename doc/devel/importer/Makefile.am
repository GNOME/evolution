# The name of the module.
DOC_MODULE=evolution-shell-importer

# The directory containing the source code
DOC_SOURCE_DIR=$(EVOLUTION_DIR)/shell/importer

CFLAGS ="						\
	-I$(top_srcdir)/shell/importer			\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	"

LDFLAGS="								\
	$(BONOBO_VFS_GNOME_LIBS)					\
	$(top_builddir)/shell/importer/.libs/libevolution-importer.a	\
	"

DOC_DIR=$(datadir)/gnome/html

DOC_DIR_INSTALL_FILES =					\
	evolution-shell-importer.args			\
	evolution-shell-importer.hierarchy		\
	evolution-shell-importer.signals		\
	evolution-shell-importer.types			\
	evolution-shell-importer-decl.txt		\
	evolution-shell-importer-sections.txt

SOURCE_FILES =								\
	$(top_srcdir)/shell/importer/evolution-importer.c		\
	$(top_srcdir)/shell/importer/evolution-importer.h		\
	$(top_srcdir)/shell/importer/evolution-importer-client.c	\
	$(top_srcdir)/shell/importer/evolution-importer-client.h

IGNORED_HEADER_FILES =			\
	GNOME_Evolution_Importer.h	\
	importer.h

scan_generated =						\
	evolution-shell-importer-decl.txt			\
	evolution-shell-importer.args				\
	evolution-shell-importer.hierarchy			\
	evolution-shell-importer.signals			\
	evolution-shell-importer.types

tmpl_dependencies =						\
	evolution-shell-importer-decl.txt			\
	evolution-shell-importer-sections.txt			\
	evolution-shell-importer.args				\
	evolution-shell-importer.hierarchy			\
	evolution-shell-importer.signals

tmpl_sources = 						\
	tmpl/evolution-importer.sgml			\
	tmpl/evolution-shell-importer-unused.sgml

tmpl_generated =					\
	evolution-shell-importer-unused.txt

sgml_dependencies =					\
	evolution-shell-importer-decl.txt		\
	evolution-shell-importer-sections.txt		\
	evolution-shell-importer.args			\
	evolution-shell-importer.hierarchy		\
	evolution-shell-importer.signals		\
	tmpl/evolution-importer.sgml

sgml_generated =					\
	sgml/evolution-importer.sgml			\
	sgml/evolution-shell-importer-doc.bottom	\
	sgml/evolution-shell-importer-doc.top		\
	sgml/object_index.sgml				\
	sgml/tree_index.sgml

EXTRA_DIST =							\
	evolution-shell-importer.args				\
	evolution-shell-importer.hierarchy			\
	evolution-shell-importer.signals			\
	evolution-shell-importer.types				\
	evolution-shell-importer-decl.txt			\
	evolution-shell-importer-sections.txt

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

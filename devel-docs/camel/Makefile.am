## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=camel

# The top-level SGML file.
DOC_MAIN_SGML_FILE=camel-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)/camel

CFLAGS=`gtk-config --cflags gtk`
LDFLAGS="-lcamel `gtk-config --libs gtk` "


HTML_DIR=$(datadir)/gnome/html



TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =					\
	tmpl/camel-service.sgml			
	tmpl/camel-store.sgml			


camel_docdir = $(HTML_DIR)
camel_doc_DATA =				\
	camel.html				\
	camel.hierarchy			\
	camel.types				\
	camel-scan.c				\
	camel-decl.txt			\
	camel-sections.txt

EXTRA_DIST = $(camel_doc_DATA)

camel.html: html/book1.html
	-cd $(srcdir) && cp html/book1.html camel.html

html/book1.html: sgml/camel-doc.bottom
	$(MAKE) html

sgml/camel-doc.bottom: $(tmpl_sources)
	$(MAKE) sgml

camel-scan.c:

scan: camel-scan.c				
	-CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) gtkdoc-scanobj --module=$(DOC_MODULE)
	gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

templates: camel-scan.c
	gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml: templates
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html: sgml
	if ! test -d html ; then mkdir html ; fi
	-cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *.args *-unused.txt camel.html

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt Makefile.in 

install-data-local: 
	$(mkinstalldirs) $(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(TARGET_DIR); \
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
	fi)

dist-hook:
	mkdir $(distdir)/html
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml


.PHONY : html sgml templates scan

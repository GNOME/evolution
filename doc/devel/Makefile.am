SUBDIRS = calendar

# The name of the module.
DOC_MODULE=evolution-devel-guide

# The top-level SGML file.
DOC_MAIN_SGML_FILE=evolution-devel-guide.sgml

HTML_DIR=$(datadir)/gnome/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

evolution_devel_guidedir = $(HTML_DIR)
evolution_devel_guide_DATA =			\
	evolution-devel-guide.html

content_files =					\
	evolution-devel-guide.sgml

EXTRA_DIST = 					\
	$(evolution_devel_guide_DATA)		\
	$(content_files)

if ENABLE_GTK_DOC
evolution-devel-guide.html: html/book1.html
	-cd $(srcdir) && cp html/book1.html evolution-devel-guide.html
else
evolution-devel-guide.html:
endif

html/book1.html: $(content-files)
	$(MAKE) html

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local:
	cd $(srcdir) && rm -rf html

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
	fi)

dist-hook:
	mkdir $(distdir)/html
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html

.PHONY: html

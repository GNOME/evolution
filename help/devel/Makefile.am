SUBDIRS = calendar

# The name of the module.
DOC_MODULE=evolution-devel-guide

# The top-level SGML file.
DOC_MAIN_SGML_FILE=evolution-devel-guide.sgml

HTML_DIR=$(datadir)/gnome/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

# Add your toplevel files here

content_files =				\
	evolution-devel-guide.sgml	\
	fdl.sgml			\
	preface.sgml			\
	reference.sgml

# Add your module's hand-written and auto-generated files here; these
# are used for dependency tracking.

local_entities =					\
	calendar/alarm-generation.sgml			\
	calendar/architecture.sgml			\
	calendar/evolution-calendar.sgml		\
	calendar/public-reference.sgml			\
							\
	calendar/cal-client/sgml/cal-client.sgml	\
							\
	calendar/cal-util/sgml/cal-component.sgml	\
	calendar/cal-util/sgml/cal-recur.sgml		\
	calendar/cal-util/sgml/cal-util.sgml		\
	calendar/cal-util/sgml/timeutil.sgml

EXTRA_DIST =			\
	$(content_files)

all: html/index.html

if ENABLE_GTK_DOC
html/index.html: $(content_files) $(local_entities)
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
else
html/index.html:
endif

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

# The name of the module.
DOC_MODULE=evolution-cal-client

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(EVOLUTION_DIR)/calendar/cal-client

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
	$(top_builddir)/calendar/cal-client/.libs/libcal-client.a	\
	$(top_builddir)/calendar/cal-util/.libs/libcal-util.a		\
	$(top_builddir)/libical/src/libical/.libs/libical.a		\
	$(top_builddir)/libversit/.libs/libversit.al			\
	"

DOC_DIR=$(datadir)/gnome/html

DOC_DIR_INSTALL_FILES =				\
	 evolution-cal-client.args		\
	 evolution-cal-client.hierarchy		\
	 evolution-cal-client.signals		\
	 evolution-cal-client.types		\
	 evolution-cal-client-decl.txt		\
	 evolution-cal-client-sections.txt

SOURCE_FILES =							\
	$(top_srcdir)/calendar/cal-client/cal-client.c		\
	$(top_srcdir)/calendar/cal-client/cal-client.h		\
	$(top_srcdir)/calendar/cal-client/cal-client-types.c	\
	$(top_srcdir)/calendar/cal-client/cal-client-types.h

IGNORED_HEADER_FILES =		\
	cal-listener.h		\
	evolution-calendar.h

scan_generated =				\
	evolution-cal-client-decl.txt		\
	evolution-cal-client.args		\
	evolution-cal-client.hierarchy		\
	evolution-cal-client.signals		\
	evolution-cal-client.types

tmpl_dependencies =				\
	evolution-cal-client-decl.txt		\
	evolution-cal-client-sections.txt	\
	evolution-cal-client.args		\
	evolution-cal-client.hierarchy		\
	evolution-cal-client.signals

tmpl_sources =					\
	tmpl/cal-client.sgml			\
	tmpl/evolution-cal-client-unused.sgml

tmpl_generated =				\
	evolution-cal-client-unused.txt

sgml_dependencies =				\
	evolution-cal-client-decl.txt		\
	evolution-cal-client-sections.txt	\
	evolution-cal-client.args		\
	evolution-cal-client.hierarchy		\
	evolution-cal-client.signals		\
	tmpl/cal-client.sgml

sgml_generated = 				\
	sgml/cal-client.sgml			\
	sgml/evolution-cal-client-doc.bottom	\
	sgml/evolution-cal-client-doc.top	\
	sgml/object_index.sgml			\
	sgml/tree_index.sgml

EXTRA_DIST = 					\
	 evolution-cal-client.args		\
	 evolution-cal-client.hierarchy		\
	 evolution-cal-client.signals		\
	 evolution-cal-client.types		\
	 evolution-cal-client-decl.txt		\
	 evolution-cal-client-sections.txt

all: $(sgml_generated)

if ENABLE_GTK_DOC
scan $(scan_generated): $(SOURCE_FILES)
	-(ln -sf $(srcdir)/$(DOC_MODULE).types .					\
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
	for i in $(DOC_DIR_INSTALL_FILES); do		  \
		$(INSTALL_DATA) $(srcdir)/$$i $(DOC_DIR); \
	done

dist-hook:
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY: scan templates sgml

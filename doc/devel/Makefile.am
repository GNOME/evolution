## Process this file with automake to produce Makefile.in

SUBDIRS = calendar 

# The name of the module, e.g. 'glib'.
DOC_MODULE=evolution-devel-guide

# The top-level SGML file. Change it if you want.
DOC_MAIN_SGML_FILE=evolution-devel-guide.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
#DOC_SOURCE_DIR=

# Extra options to supply to gtkdoc-scan.
#SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
#MKDB_OPTIONS=

# Extra options to supply to gtkdoc-fixref.
#FIXXREF_OPTIONS=

# Used for dependencies.
#HFILE_GLOB=
#CFILE_GLOB=

# Header files to ignore when scanning.
#IGNORE_HFILES=

# Images to copy into HTML directory.
HTML_IMAGES =


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

#	executive-summary/sgml/executive-summary-component.sgml	\
#	executive-summary/sgml/executive-summary-component-factory.sgml	\
#	executive-summary/sgml/executive-summary-component-factory-client.sgml	\
#	executive-summary/sgml/executive-summary-html-view.sgml

#	importer/sgml/evolution-importer.sgml		\
#	importer/sgml/evolution-importer-client.sgml

installed_content_files = 		\
	fdl.sgml			\
	preface.sgml			\
	reference.sgml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =				\
	$(installed_content_files)	\
	$(local_entities)

# Other files to distribute.
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed if your app/lib
# contains GtkObjects/GObjects and you want to document signals and properties.
#GTKDOC_CFLAGS =
#GTKDOC_LIBS =
#
#GTKDOC_CC=$(LIBTOOL) --mode=compile $(CC)
#GTKDOC_LD=$(LIBTOOL) --mode=link $(CC)

# If you need to override some of the declarations, place them in this file
# and uncomment this line.
#DOC_OVERRIDES = $(DOC_MODULE)-overrides.txt

HTML_DIR = $(datadir)/gnome/html/evolution-$(BASE_VERSION)

###########################################################################
# Everything below here is generic and you shouldn't need to change it.
###########################################################################

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST =				\
	$(extra_files)		  	\
	$(installed_content_files)	\
	$(HTML_IMAGES)			\
	$(DOC_MAIN_SGML_FILE)		\
	ChangeLog.pre-1-4

#	$(DOC_MODULE).types	     \
#	$(DOC_MODULE)-sections.txt      \
#	$(DOC_OVERRIDES)

DOC_STAMPS=scan-build.stamp tmpl-build.stamp sgml-build.stamp html-build.stamp \
	   $(srcdir)/tmpl.stamp $(srcdir)/sgml.stamp $(srcdir)/html.stamp

#SCANOBJ_FILES =		 \
#	$(DOC_MODULE).args      \
#	$(DOC_MODULE).hierarchy \
#	$(DOC_MODULE).signals

if ENABLE_GTK_DOC
all-local: html-build.stamp

#### html ####

html-build.stamp: $(DOC_MAIN_SGML_FILE) $(content_files) #sgml.stamp
	@echo '*** Building HTML ***'
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" = "x" || ( cd $(srcdir) && cp $(HTML_IMAGES) html )
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp
endif

##############

clean-local:
	rm -f *~ *.bak *-unused.txt $(DOC_STAMPS) # $(SCANOBJ_FILES)

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

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
	fi)

#	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
#	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/html
	files=`echo $(srcdir)/html/*.html` ; \
	test '$(srcdir)/html/*.html' = "$$files" \
	    || cp $$files $(distdir)/html
	files=`echo $(srcdir)/html/*.css` ; \
	test '$(srcdir)/html/*.css' = "$$files" \
	    || cp $$files $(distdir)/html
	images=$(HTML_IMAGES) ;	       \
	for i in $$images ; do		\
	  cp $(srcdir)/$$i $(distdir)/html ;  \
	done

#	mkdir $(distdir)/tmpl
#	mkdir $(distdir)/sgml
#	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
#	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl

.PHONY : dist-hook-local

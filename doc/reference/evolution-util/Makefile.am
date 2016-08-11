# The name of the module.
DOC_MODULE = evolution-util

# The top-level SGML file.
DOC_MAIN_SGML_FILE = evolution-util-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
DOC_SOURCE_DIR = $(top_srcdir)/e-util

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = --rebuild-sections --rebuild-types

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = $(top_srcdir)/e-util/*.h
CFILE_GLOB = $(top_srcdir)/e-util/*.c

# Ignore all accessiblity headers.
IGNORE_HFILES = \
	e-html-editor-actions.h \
	e-html-editor-private.h \
	e-marshal.h \
	e-table-col-dnd.h \
	e-table-defines.h \
	e-util-enumtypes.h \
	e-util-private.h \
	ea-calendar-cell.h \
	ea-calendar-item.h \
	ea-cell-table.h \
	ea-factory.h \
	ea-widgets.h \
	gal-a11y-e-cell.h \
	gal-a11y-e-cell-popup.h \
	gal-a11y-e-cell-registry.h \
	gal-a11y-e-cell-text.h \
	gal-a11y-e-cell-toggle.h \
	gal-a11y-e-cell-tree.h \
	gal-a11y-e-cell-vbox.h \
	gal-a11y-e-table-click-to-add-factory.h \
	gal-a11y-e-table-click-to-add.h \
	gal-a11y-e-table-column-header.h \
	gal-a11y-e-table-factory.h \
	gal-a11y-e-table-item-factory.h \
	gal-a11y-e-table-item.h \
	gal-a11y-e-table.h \
	gal-a11y-e-text-factory.h \
	gal-a11y-e-text.h \
	gal-a11y-e-tree-factory.h \
	gal-a11y-e-tree.h \
	gal-a11y-factory.h \
	gal-a11y-util.h \
	$(NULL)

GITIGNOREFILES = \
	$(DOC_MODULE)-overrides.txt \
	$(DOC_MODULE)-sections.txt \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).types \
	$(NULL)

GTKDOC_CFLAGS  =					\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(NULL)

GTKDOC_LIBS = 						\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --sgml-mode

DISTCLEANFILES = $(GITIGNOREFILES)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

#TESTS = $(GTKDOC_CHECK)

-include $(top_srcdir)/git.mk

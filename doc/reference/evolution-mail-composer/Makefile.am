# The name of the module.
DOC_MODULE=evolution-mail-composer

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
DOC_SOURCE_DIR=$(top_srcdir)/composer

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = --rebuild-sections --rebuild-types

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB=$(top_srcdir)/composer/*.h
CFILE_GLOB=$(top_srcdir)/composer/*.c

# Header files to ignore when scanning.
IGNORE_HFILES= \
	e-composer-actions.h \
	e-composer-private.h \
	$(NULL)

GITIGNOREFILES = \
	$(DOC_MODULE)-overrides.txt \
	$(DOC_MODULE)-sections.txt \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).types \
	$(NULL)

GTKDOC_CFLAGS= \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(NULL)

GTKDOC_LIBS=								\
	$(top_builddir)/composer/libevolution-mail-composer.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(NULL)

DISTCLEANFILES = $(GITIGNOREFILES)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

#TESTS = $(GTKDOC_CHECK)

-include $(top_srcdir)/git.mk

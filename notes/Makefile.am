noinst_PROGRAMS = notes-test

INCLUDES = \
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/widgets/e-text			\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DG_LOG_DOMAIN=\"evolution-notes\"

notes_test_SOURCES = 			\
	e-bevel-button.c		\
	e-bevel-button.h		\
	e-bevel-button-util.c		\
	e-bevel-button-util.h		\
	e-note.c			\
	e-note.h			\
	test-notes.c

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Notes.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

notes_test_LDADD =					\
	$(BONOBO_HTML_GNOME_LIBS)			\
	$(EXTRA_GNOME_LIBS)

EXTRA_DIST= $(oaf_in_files) $(oaf_DATA)

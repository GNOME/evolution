bin_PROGRAMS = evolution-notes

noinst_PROGRAMS = notes-test

INCLUDES = \
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/widgets/e-text			\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DG_LOG_DOMAIN=\"evolution-notes\"

evolution_notes_SOURCES =		\
	component-factory.c		\
	main.c				\
	notes.c

notes_test_SOURCES = 			\
	e-bevel-button.c		\
	e-bevel-button.h		\
	e-bevel-button-util.c		\
	e-bevel-button-util.h		\
	e-note.c			\
	e-note.h			\
	test-notes.c

oafdir = $(datadir)/oaf
oaf_DATA = evolution-notes.oafinfo

notes_test_LDADD =					\
	$(top_builddir)/widgets/e-text/libetext.a 	\
	$(top_builddir)/e-util/libeutil.la		\
	$(BONOBO_HTML_GNOME_LIBS)			

evolution_notes_LDADD = 				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.a		\
	$(BONOBO_HTML_GNOME_LIBS)			

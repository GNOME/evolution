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

# FIXME We should make a libeshell library instead of this gross hack.
SHELL_OBJS =					\
	$(top_builddir)/shell/Evolution-common.o \
	$(top_builddir)/shell/Evolution-stubs.o	\
	$(top_builddir)/shell/Evolution-skels.o	\
	$(top_builddir)/shell/evolution-shell-component.o \
	$(top_builddir)/shell/evolution-storage.o

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

OAF_FILES = evolution-notes.oafinfo

if USING_OAF
oafdir = $(datadir)/oaf
oaf_DATA = $(OAF_FILES)
endif

notes_test_LDADD =					\
	$(top_builddir)/widgets/e-text/libetext.a 	\
	../e-util/libeutil.la				\
	$(BONOBO_HTML_GNOME_LIBS)			

evolution_notes_LDADD = 				\
	../e-util/libeutil.la				\
	$(SHELL_OBJS)					\
	$(BONOBO_HTML_GNOME_LIBS)			





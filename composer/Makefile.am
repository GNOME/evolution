## CORBA stuff

IDLS =				\
	Evolution-Composer.idl	\
	Composer.idl

IDL_GENERATED =			\
	Composer.h		\
	Composer-common.c	\
	Composer-skels.c	\
	Composer-stubs.c

HTML_EDITOR_GENERATED =		\
	Editor.h		\
	Editor-common.c	\
	Editor-skels.c	\
	Editor-stubs.c

selectnamesdir = $(top_srcdir)/addressbook/gui/component/select-names

$(IDL_GENERATED): $(IDLS) $(selectnamesdir)/Evolution-Addressbook-SelectNames.idl
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		-I $(selectnamesdir)  $(srcdir)/Composer.idl

Editor-commmon.c:	$(GTK_HTML_EDITOR_IDL)

$(HTML_EDITOR_GENERATED): $(GTK_HTML_EDITOR_IDL)
	$(ORBIT_IDL) -I $(srcdir) `$(GNOME_CONFIG) --cflags idl` $(GTK_HTML_EDITOR_IDL)

##

idldir = $(datadir)/idl
idl_DATA = $(IDLS)

gladedir = $(datadir)/evolution/glade

glade_DATA =					\
	e-msg-composer-attachment.glade

libcomposerincludedir = $(includedir)/composer

noinst_LIBRARIES = libcomposer.a

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
	-I$(top_builddir)/widgets					\
	-I$(top_srcdir)/camel					 	\
	-I$(top_builddir)/camel					 	\
	-I$(top_builddir)/addressbook/backend/ebook			\
	-I$(top_builddir)/addressbook/gui/component/select-names	\
	-I$(top_builddir)/shell						\
	-I$(top_srcdir)/shell						\
	$(EXTRA_GNOME_CFLAGS)						\
	$(GNOME_INCLUDEDIR)						\
	$(BONOBO_GNOME_CFLAGS)						\
	$(GTKHTML_CFLAGS)						\
	-DEVOLUTION_DATADIR=\"$(datadir)\"				\
	-DE_GLADEDIR=\"$(gladedir)\"					\
	-DG_LOG_DOMAIN=\"composer\"

libcomposer_a_SOURCES = 			\
	$(IDL_GENERATED)			\
	$(HTML_EDITOR_GENERATED)		\
	e-msg-composer-attachment-bar.c		\
	e-msg-composer-attachment-bar.h		\
	e-msg-composer-attachment.c		\
	e-msg-composer-attachment.h		\
	e-msg-composer-hdrs.c			\
	e-msg-composer-hdrs.h			\
	e-msg-composer-select-file.c		\
	e-msg-composer-select-file.h		\
	e-msg-composer.c			\
	e-msg-composer.h			\
	e-icon-list.c				\
	e-icon-list.h				\
	evolution-composer.c			\
	evolution-composer.h			\
	listener.c				\
	listener.h

EXTRA_DIST =					\
	$(glade_DATA)				\
	$(IDLS)					\
	bad-icon.xpm

BUILT_SOURCES = $(IDL_GENERATED) $(HTML_EDITOR_GENERATED)
CLEANFILES   += $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

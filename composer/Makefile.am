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
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) \
		-I $(selectnamesdir)  $(srcdir)/Composer.idl

Editor-commmon.c: $(GTKHTML_DATADIR)/Editor.idl

$(HTML_EDITOR_GENERATED): $(GTKHTML_DATADIR)/Editor.idl
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) -I $(GTKHTML_DATADIR)/gtkhtml $(GTKHTML_DATADIR)/Editor.idl

##

composer-marshal.h: composer-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=e_msg_composer_marshal composer-marshal.list --header > composer-marshal.tmp \
	&& mv composer-marshal.tmp composer-marshal.h ) \
	|| ( rm -f composer-marshal.tmp && exit 1 )
composer-marshal.c: composer-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=e_msg_composer_marshal composer-marshal.list --body > composer-marshal.tmp \
	&& mv composer-marshal.tmp composer-marshal.c ) \
	|| ( rm -f composer-marshal.tmp && exit 1 )

$(libcomposer_la_OBJECTS): composer-marshal.h

##

idl_DATA = $(IDLS)

glade_DATA =					\
	e-msg-composer-attachment.glade

noinst_LTLIBRARIES = libcomposer.la

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
	-I$(top_builddir)/widgets					\
	-I$(top_srcdir)/camel					 	\
	-I$(top_builddir)/camel					 	\
	-I$(top_srcdir)/addressbook/backend				\
	-I$(top_builddir)/addressbook/backend				\
	-I$(top_builddir)/addressbook/gui/component/select-names	\
	-I$(top_builddir)/shell						\
	-I$(top_srcdir)/shell						\
	-DEVOLUTION_DATADIR=\"$(datadir)\"				\
	-DEVOLUTION_UIDIR=\"$(evolutionuidir)\"				\
	-DEVOLUTION_GLADEDIR=\"$(gladedir)\"				\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"composer\"					\
	-DG_DISABLE_DEPRECATED						\
	-DGTK_DISABLE_DEPRECATED					\
	$(EVOLUTION_MAIL_CFLAGS)

libcomposer_la_SOURCES = 			\
	$(IDL_GENERATED)			\
	$(HTML_EDITOR_GENERATED)		\
	composer-marshal.c			\
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
	evolution-composer.c			\
	evolution-composer.h			\
	listener.c				\
	listener.h

EXTRA_DIST =					\
	$(glade_DATA)				\
	$(IDLS)					\
	ChangeLog.pre-1-4			\
	composer-marshal.list			\
	bad-icon.xpm

BUILT_SOURCES = $(IDL_GENERATED) $(HTML_EDITOR_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

## CORBA stuff

IDLS =				\
	Evolution-Composer.idl	\
	Composer.idl		\
	HTMLEditor.idl

IDL_GENERATED =			\
	Composer.h		\
	Composer-common.c	\
	Composer-skels.c	\
	Composer-stubs.c	\
	HTMLEditor.h		\
	HTMLEditor-common.c	\
	HTMLEditor-skels.c	\
	HTMLEditor-stubs.c

selectnamesdir = $(top_srcdir)/addressbook/gui/component/select-names

$(IDL_GENERATED): $(IDLS) $(selectnamesdir)/Evolution-Addressbook-SelectNames.idl
	$(ORBIT_IDL) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		-I $(selectnamesdir) -I . $(srcdir)/Composer.idl
	$(ORBIT_IDL) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`	\
		-I $(selectnamesdir) -I . $(srcdir)/HTMLEditor.idl

##

gladedir = $(datadir)/evolution/glade

glade_DATA =					\
	e-msg-composer-attachment.glade

glade_messages = \
	e-msg-composer-attachment.glade.h

libcomposerincludedir = $(includedir)/composer

noinst_LTLIBRARIES = libcomposer.la
libcomposer_la_LDFLAGS = -static

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/widgets						\
	-I$(top_builddir)/widgets					\
	-I$(top_srcdir)/camel					 	\
	-I$(top_builddir)/camel					 	\
	-I$(top_builddir)/addressbook/gui/component/select-names	\
	-I$(top_builddir)/shell						\
	$(EXTRA_GNOME_CFLAGS)						\
	$(GNOME_INCLUDEDIR)						\
	$(BONOBO_GNOME_CFLAGS)						\
	$(GTKHTML_CFLAGS)						\
	$(UNICODE_CFLAGS)						\
	-DEVOLUTION_DATADIR=\"$(datadir)\"				\
	-DE_GLADEDIR=\"$(gladedir)\"					\
	-DG_LOG_DOMAIN=\"composer\"

libcomposer_la_SOURCES = 			\
	$(IDL_GENERATED)			\
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
	$(glade_messages)			\
	bad-icon.xpm

BUILT_SOURCES = $(IDL_GENERATED)
CLEANFILES   += $(BUILT_SOURCES)

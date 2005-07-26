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

$(IDL_GENERATED): $(IDLS)
	$(ORBIT_IDL) -I $(srcdir) -I $(datadir)/idl $(IDL_INCLUDES) \
		$(srcdir)/Composer.idl

Editor-commmon.c: $(GTKHTML_DATADIR)/Editor.idl

$(HTML_EDITOR_GENERATED): $(GTKHTML_DATADIR)/Editor.idl
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) -I $(GTKHTML_DATADIR)/gtkhtml $(GTKHTML_DATADIR)/Editor.idl

##

error_DATA = mail-composer.error
errordir = $(privdatadir)/errors

# provides error rule
@EVO_PLUGIN_RULE@

idl_DATA = $(IDLS)

noinst_LTLIBRARIES = libcomposer.la

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_builddir)/composer					\	
	-I$(top_srcdir)/widgets						\
	-I$(top_builddir)/widgets					\
	-I$(top_srcdir)/widgets/misc					\
	-I$(top_builddir)/widgets/misc					\
	-I$(top_builddir)/shell						\
	-I$(top_srcdir)/shell						\
	-DEVOLUTION_DATADIR=\"$(datadir)\"				\
	-DEVOLUTION_IMAGESDIR=\"$(imagesdir)\"                          \
	-DEVOLUTION_UIDIR=\"$(evolutionuidir)\"				\
	-DEVOLUTION_GLADEDIR=\"$(gladedir)\"				\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"composer\"					\
	$(EVOLUTION_MAIL_CFLAGS)

libcomposer_la_SOURCES = 			\
	$(IDL_GENERATED)			\
	$(HTML_EDITOR_GENERATED)		\
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
	mail-composer.error.xml			\
	$(IDLS)					\
	ChangeLog.pre-1-4

BUILT_SOURCES = $(IDL_GENERATED) $(HTML_EDITOR_GENERATED) $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

error_DATA = mail-composer.error
errordir = $(privdatadir)/errors

# provides error rule
@EVO_PLUGIN_RULE@

noinst_LTLIBRARIES = libcomposer.la
libcomposerincludedir = $(privincludedir)/composer
libcomposerinclude_HEADERS = 			\
	e-composer-header.h			\
	e-composer-header-table.h		\
	e-composer-from-header.h		\
	e-composer-name-header.h		\
	e-composer-post-header.h		\
	e-composer-private.h			\
	e-composer-text-header.h		\
	e-composer-common.h			\
	e-composer-actions.h			\
	e-composer-autosave.h			\
	e-msg-composer.h

AM_CPPFLAGS =								\
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
	$(libcomposerinclude_HEADERS)		\
	e-composer-actions.c			\
	e-composer-autosave.c			\
	e-composer-header.c			\
	e-composer-header-table.c		\
	e-composer-from-header.c		\
	e-composer-name-header.c		\
	e-composer-post-header.c		\
	e-composer-private.c			\
	e-composer-text-header.c		\
	e-msg-composer.c


uidir = $(evolutionuidir)
ui_DATA = evolution-composer.ui

EXTRA_DIST =					\
	$(ui_DATA)				\
	mail-composer.error.xml			\
	ChangeLog.pre-1-4

BUILT_SOURCES = $(IDL_GENERATED) $(HTML_EDITOR_GENERATED) $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

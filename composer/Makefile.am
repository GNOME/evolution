error_DATA = mail-composer.error
errordir = $(privdatadir)/errors

# provides error rule
@EVO_PLUGIN_RULE@

privsolib_LTLIBRARIES = libevolution-mail-composer.la

evolution_mail_composer_includedir = $(privincludedir)/composer

evolution_mail_composer_include_HEADERS = 	\
	e-composer-actions.h			\
	e-composer-common.h			\
	e-composer-from-header.h		\
	e-composer-header-table.h		\
	e-composer-header.h			\
	e-composer-name-header.h		\
	e-composer-post-header.h		\
	e-composer-private.h			\
	e-composer-spell-header.h		\
	e-composer-text-header.h		\
	e-msg-composer.h

libevolution_mail_composer_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_builddir)/composer					\
	-I$(top_builddir)/shell						\
	-I$(top_srcdir)/shell						\
	-DEVOLUTION_DATADIR=\"$(datadir)\"				\
	-DEVOLUTION_IMAGESDIR=\"$(imagesdir)\"				\
	-DEVOLUTION_UIDIR=\"$(uidir)\"					\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"composer\"					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)						\
	$(NULL)

libevolution_mail_composer_la_SOURCES =		\
	$(evolution_mail_composer_include_HEADERS) \
	e-composer-actions.c			\
	e-composer-from-header.c		\
	e-composer-header-table.c		\
	e-composer-header.c			\
	e-composer-name-header.c		\
	e-composer-post-header.c		\
	e-composer-private.c			\
	e-composer-spell-header.c		\
	e-composer-text-header.c		\
	e-msg-composer.c

libevolution_mail_composer_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolution_mail_composer_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/shell/libevolution-shell.la	\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(top_builddir)/em-format/libevolution-mail-formatter.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la		\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

ui_DATA = evolution-composer.ui

EXTRA_DIST =					\
	$(ui_DATA)				\
	mail-composer.error.xml

BUILT_SOURCES = $(error_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

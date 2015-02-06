privsolib_LTLIBRARIES = libevolution-mail-importers.la

libevolution_mail_importers_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I.. 						\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"	\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

libevolution_mail_importers_la_SOURCES =	\
	kmail-libs.c				\
	kmail-libs.h				\
	kmail-importer.c			\
	mail-importer.c				\
	mail-importer.h				\
	elm-importer.c				\
	pine-importer.c				\
	evolution-mbox-importer.c

libevolution_mail_importers_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolution_mail_importers_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(NULL)

-include $(top_srcdir)/git.mk

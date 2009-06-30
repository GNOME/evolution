if OS_WIN32
WIN32_BOOTSTRAP_LIBS = \
	$(top_builddir)/win32/libevolution-mail-shared.la \
	$(top_builddir)/win32/libevolution-mail.la
endif

privsolib_LTLIBRARIES = libevolution-mail-importers.la

AM_CPPFLAGS = -I.. 					\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"	\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(IMPORTERS_CFLAGS)

libevolution_mail_importers_la_SOURCES =	\
	mail-importer.c				\
	mail-importer.h				\
	elm-importer.c				\
	pine-importer.c				\
	evolution-mbox-importer.c

libevolution_mail_importers_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_mail_importers_la_LIBADD =				\
	$(WIN32_BOOTSTRAP_LIBS)					\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(IMPORTERS_LIBS)

-include $(top_srcdir)/git.mk

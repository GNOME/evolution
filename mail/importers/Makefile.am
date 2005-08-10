if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libevolution-mail.la
endif

privsolib_LTLIBRARIES = libevolution-mail-importers.la

INCLUDES = -I.. 					\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"evolution-mail-importer\"	\
	-I$(top_srcdir)/addressbook/backend             \
	-I$(top_builddir)/addressbook/backend           \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(IMPORTERS_CFLAGS)

libevolution_mail_importers_la_SOURCES =	\
	mail-importer.c				\
	mail-importer.h				\
	elm-importer.c				\
	pine-importer.c				\
	evolution-mbox-importer.c

libevolution_mail_importers_la_LDFLAGS = $(NO_UNDEFINED)

# these haven't been converted to plugins yet
#	netscape-importer.c
#	evolution-outlook-importer.c

libevolution_mail_importers_la_LIBADD =				\
	$(WIN32_BOOTSTRAP_LIBS)					\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(IMPORTERS_LIBS)

privsolib_LTLIBRARIES = libevolution-mail-importers.la

libevolution_mail_importers_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I.. 						\
	-I$(srcdir)/..					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			\
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
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(GNOME_PLATFORM_LIBS)					\
	$(IMPORTERS_LIBS)

-include $(top_srcdir)/git.mk

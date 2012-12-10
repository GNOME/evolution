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
	$(CHAMPLAIN_CFLAGS)				\
	$(GTKHTML_CFLAGS)

libevolution_mail_importers_la_SOURCES =	\
	mail-importer.c				\
	mail-importer.h				\
	elm-importer.c				\
	pine-importer.c				\
	evolution-mbox-importer.c

libevolution_mail_importers_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

libevolution_mail_importers_la_LIBADD =				\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(CHAMPLAIN_LIBS)					\
	$(GTKHTML_LIBS)

-include $(top_srcdir)/git.mk

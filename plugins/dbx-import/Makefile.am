if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(EVOLUTION_CALENDAR_LIBS)			\
	$(EVOLUTION_MAIL_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-dbx-import.eplug

plugin_LTLIBRARIES = liborg-gnome-dbx-import.la

liborg_gnome_dbx_import_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)				\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	-DLOCALEDIR="\"$(LOCALEDIR)\""			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_CFLAGS)				\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(CAMEL_CFLAGS)

liborg_gnome_dbx_import_la_SOURCES = dbx-importer.c

liborg_gnome_dbx_import_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_dbx_import_la_LIBADD =			\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(CAMEL_LIBS)					\
	$(EVOLUTION_CALENDAR_LIBS)			\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)				\
	$(CAMEL_LIBS)

EXTRA_DIST = org-gnome-dbx-import.eplug.xml

BUILT_SOURCES = org-gnome-dbx-import.eplug

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk

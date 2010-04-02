if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(EVOLUTION_CALENDAR_LIBS)			\
	$(EVOLUTION_MAIL_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-pst-import.eplug

plugin_LTLIBRARIES = liborg-gnome-pst-import.la

liborg_gnome_pst_import_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)				\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	-DLOCALEDIR="\"$(LOCALEDIR)\""			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_CFLAGS)				\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(LIBPST_CFLAGS)				\
	$(CAMEL_CFLAGS)

liborg_gnome_pst_import_la_SOURCES = pst-importer.c

liborg_gnome_pst_import_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_pst_import_la_LIBADD =			\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(CAMEL_LIBS)					\
	$(EVOLUTION_CALENDAR_LIBS)			\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)				\
	$(LIBPST_LIBS)					\
	$(CAMEL_LIBS)

EXTRA_DIST = org-gnome-pst-import.eplug.xml

BUILT_SOURCES = org-gnome-pst-import.eplug

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk

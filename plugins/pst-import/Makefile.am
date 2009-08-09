if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(EVOLUTION_CALENDAR_LIBS)			\
	$(EVOLUTION_MAIL_LIBS)
endif

AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)				\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	-DLOCALEDIR="\"$(LOCALEDIR)\""			\
	$(EVOLUTION_CFLAGS)				\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(LIBPST_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-pst-import.eplug
plugin_LTLIBRARIES = liborg-gnome-pst-import.la

liborg_gnome_pst_import_la_SOURCES = pst-importer.c
liborg_gnome_pst_import_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)
liborg_gnome_pst_import_la_LIBADD =	\
	$(NO_UNDEFINED_REQUIRED_LIBS)	\
	$(LIBPST_LIBS)

EXTRA_DIST = org-gnome-pst-import.eplug.xml

BUILT_SOURCES = org-gnome-pst-import.eplug

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk

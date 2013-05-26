if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(EVOLUTION_MAIL_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-pst-import.eplug

plugin_LTLIBRARIES = liborg-gnome-pst-import.la

liborg_gnome_pst_import_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)				\
	$(LIBPST_CFLAGS)

liborg_gnome_pst_import_la_SOURCES = pst-importer.c

liborg_gnome_pst_import_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_pst_import_la_LIBADD =				\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(LIBPST_LIBS)

EXTRA_DIST = org-gnome-pst-import.eplug.xml

BUILT_SOURCES = org-gnome-pst-import.eplug

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

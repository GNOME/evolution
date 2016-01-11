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
	$(LIBPST_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_pst_import_la_SOURCES = pst-importer.c

liborg_gnome_pst_import_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

liborg_gnome_pst_import_la_LIBADD =				\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LIBPST_LIBS)						\
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@
metainfodir = $(datadir)/appdata
metainfo_in_files = evolution-pst.metainfo.xml.in
metainfo_files = $(metainfo_in_files:.xml.in=.xml)
dist_metainfo_DATA = $(metainfo_files)

EXTRA_DIST = org-gnome-pst-import.eplug.xml $(metainfo_in_files)

BUILT_SOURCES = org-gnome-pst-import.eplug

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(metainfo_files)

-include $(top_srcdir)/git.mk

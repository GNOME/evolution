if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-dbx-import.eplug

plugin_LTLIBRARIES = liborg-gnome-dbx-import.la

liborg_gnome_dbx_import_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CHAMPLAIN_CFLAGS)				\
	$(GTKHTML_CFLAGS)

liborg_gnome_dbx_import_la_SOURCES = dbx-importer.c

liborg_gnome_dbx_import_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_dbx_import_la_LIBADD =			\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(CHAMPLAIN_LIBS)				\
	$(GTKHTML_LIBS)

EXTRA_DIST = org-gnome-dbx-import.eplug.xml

BUILT_SOURCES = org-gnome-dbx-import.eplug

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk

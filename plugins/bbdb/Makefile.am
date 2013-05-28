if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(GNOME_PLATFORM_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-bbdb.eplug

plugin_LTLIBRARIES = liborg-gnome-evolution-bbdb.la

liborg_gnome_evolution_bbdb_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_builddir)/composer			\
	-I$(top_srcdir)					\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_evolution_bbdb_la_SOURCES = bbdb.c bbdb.h gaimbuddies.c

liborg_gnome_evolution_bbdb_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_evolution_bbdb_la_LIBADD =		\
	$(top_builddir)/e-util/libevolution-util.la \
	$(top_builddir)/shell/libevolution-shell.la \
	$(top_builddir)/composer/libevolution-mail-composer.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(EVOLUTION_DATA_SERVER_LIBS)		\
	$(GNOME_PLATFORM_LIBS)			\
	$(GTKHTML_LIBS)

EXTRA_DIST = org-gnome-evolution-bbdb.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

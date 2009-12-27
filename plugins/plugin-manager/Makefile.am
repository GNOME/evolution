@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-plugin-manager.eplug

plugin_LTLIBRARIES = liborg-gnome-plugin-manager.la

liborg_gnome_plugin_manager_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

liborg_gnome_plugin_manager_la_SOURCES = plugin-manager.c

liborg_gnome_plugin_manager_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_plugin_manager_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = \
	org-gnome-plugin-manager.eplug.xml

BUILT_SOURCES = org-gnome-plugin-manager.eplug

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

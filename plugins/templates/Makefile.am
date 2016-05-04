@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-templates.eplug

plugin_LTLIBRARIES = liborg-gnome-templates.la

liborg_gnome_templates_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_builddir)/composer			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_templates_la_SOURCES =			\
	templates.c					\
	e-templates-store.h				\
	e-templates-store.c

liborg_gnome_templates_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

liborg_gnome_templates_la_LIBADD =	\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/composer/libevolution-mail-composer.la \
	$(top_builddir)/shell/libevolution-shell.la	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

EXTRA_DIST = 	org-gnome-templates.eplug.xml

BUILT_SOURCES = org-gnome-templates.eplug

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

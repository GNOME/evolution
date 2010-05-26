@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-sa-junk-plugin.eplug

plugin_LTLIBRARIES = liborg-gnome-sa-junk-plugin.la

liborg_gnome_sa_junk_plugin_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

liborg_gnome_sa_junk_plugin_la_SOURCES = em-junk-filter.c

liborg_gnome_sa_junk_plugin_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_sa_junk_plugin_la_LIBADD =	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

BUILT_SOURCES = $(plugin_DATA) $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = org-gnome-sa-junk-plugin.eplug.xml

-include $(top_srcdir)/git.mk

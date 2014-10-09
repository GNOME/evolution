module_LTLIBRARIES = module-spamassassin.la

module_spamassassin_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-spamassassin\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)

module_spamassassin_la_SOURCES = \
	evolution-spamassassin.c

module_spamassassin_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

module_spamassassin_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

@INTLTOOL_XML_RULE@
metainfodir = $(datadir)/appdata
metainfo_in_files = evolution-spamassassin.metainfo.xml.in
metainfo_files = $(metainfo_in_files:.xml.in=.xml)
dist_metainfo_DATA = $(metainfo_files)

EXTRA_DIST = $(metainfo_in_files)

DISTCLEANFILES = $(metainfo_files)

-include $(top_srcdir)/git.mk

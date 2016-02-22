@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mailing-list-actions.eplug
plugin_LTLIBRARIES = liborg-gnome-mailing-list-actions.la

liborg_gnome_mailing_list_actions_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_builddir)/composer 			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_mailing_list_actions_la_SOURCES = mailing-list-actions.c

liborg_gnome_mailing_list_actions_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

liborg_gnome_mailing_list_actions_la_LIBADD =		\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/composer/libevolution-mail-composer.la \
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/shell/libevolution-shell.la	\
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(top_builddir)/em-format/libevolution-mail-formatter.la        \
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

error_DATA = org-gnome-mailing-list-actions.error
errordir = $(privdatadir)/errors

BUILT_SOURCES = \
	org-gnome-mailing-list-actions.eplug		\
	org-gnome-mailing-list-actions.error

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = 						\
	org-gnome-mailing-list-actions.eplug.xml	\
	org-gnome-mailing-list-actions.error.xml

-include $(top_srcdir)/git.mk

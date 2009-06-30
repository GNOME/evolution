AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)/composer 			\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mailing-list-actions.eplug org-gnome-mailing-list-actions.xml
plugin_LTLIBRARIES = liborg-gnome-mailing-list-actions.la

liborg_gnome_mailing_list_actions_la_SOURCES = mailing-list-actions.c
liborg_gnome_mailing_list_actions_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_mailing_list_actions_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

error_DATA = org-gnome-mailing-list-actions.error
errordir = $(privdatadir)/errors

BUILT_SOURCES = \
	org-gnome-mailing-list-actions.eplug		\
	org-gnome-mailing-list-actions.error

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = 						\
	org-gnome-mailing-list-actions.eplug.xml	\
	org-gnome-mailing-list-actions.error.xml	\
	org-gnome-mailing-list-actions.xml

-include $(top_srcdir)/git.mk

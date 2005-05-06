INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mailing-list-actions.eplug org-gnome-mailing-list-actions.xml
plugin_LTLIBRARIES = liborg-gnome-mailing-list-actions.la

liborg_gnome_mailing_list_actions_la_SOURCES = mailing-list-actions.c
liborg_gnome_mailing_list_actions_la_LDFLAGS = -module -avoid-version

error_DATA = org-gnome-mailing-list-actions.error
errordir = $(privdatadir)/errors

BUILT_SOURCES = \
	org-gnome-mailing-list-actions.eplug		\
	org-gnome-mailing-list-actions.error

EXTRA_DIST = 						\
	org-gnome-mailing-list-actions.eplug.xml	\
	org-gnome-mailing-list-actions.error.xml	\
	org-gnome-mailing-list-actions.xml

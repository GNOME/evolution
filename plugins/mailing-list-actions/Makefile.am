INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mailing-list-actions.eplug org-gnome-mailing-list-actions.xml
plugin_LTLIBRARIES = liborg-gnome-mailing-list-actions.la

liborg_gnome_mailing_list_actions_la_SOURCES = mailing-list-actions.c
liborg_gnome_mailing_list_actions_la_LDFLAGS = -module -avoid-version

error_DATA = org-gnome-mailing-list-actions-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

BUILT_SOURCES = $(error_i18n)
EXTRA_DIST = 						\
	$(error_DATA)					\
	$(error_i18n)					\
	org-gnome-mailing-list-actions.eplug.in		\
	org-gnome-mailing-list-actions.xml

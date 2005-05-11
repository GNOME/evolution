INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-itip-formatter.eplug
plugin_LTLIBRARIES = liborg-gnome-itip-formatter.la

liborg_gnome_itip_formatter_la_SOURCES = itip-formatter.c itip-view.c itip-view.h
liborg_gnome_itip_formatter_la_LDFLAGS = -module -avoid-version

error_DATA = org-gnome-itip-formatter.error
errordir = $(privdatadir)/errors

BUILT_SOURCES = $(plugin_DATA) $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-itip-formatter.eplug.xml \
	org-gnome-itip-formatter.error.xml



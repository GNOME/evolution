INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-itip-formatter.eplug
plugin_LTLIBRARIES = liborg-gnome-itip-formatter.la

liborg_gnome_itip_formatter_la_SOURCES = itip-formatter.c itip-view.c itip-view.h
liborg_gnome_itip_formatter_la_LDFLAGS = -module -avoid-version

error_DATA = org-gnome-itip-formatter-errors.xml
error_i18n = $(error_DATA:.xml=.xml.h)
errordir = $(privdatadir)/errors
%.xml.h: %.xml
	$(top_builddir)/e-util/e-error-tool $^

BUILT_SOURCES = $(plugin_DATA) $(error_i18n)

EXTRA_DIST = $(error_DATA) $(error_i18n) org-gnome-itip-formatter.eplug.in


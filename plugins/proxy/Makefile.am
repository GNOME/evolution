INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/camel                           \
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"		

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-proxy.eplug
plugin_LTLIBRARIES = liborg-gnome-proxy.la

liborg_gnome_proxy_la_SOURCES = proxy.c
liborg_gnome_proxy_la_LDFLAGS = -module -avoid-version
liborg_gnome_proxy_la_LIBADD =				\
	$(EVOLUTION_MAIL_LIBS)				\
	$(CAMEL_GROUPWISE_LIBS)

glade_DATA =	\
	proxy-add-dialog.glade	\
	proxy-listing.glade

error_DATA = org-gnome-proxy-errors.xml
errordir = $(privdatadir)/errors

BUILT_SOURCES = $(error_i18n)

EXTRA_DIST = org-gnome-proxy.eplug.xml

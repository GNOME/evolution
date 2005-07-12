INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-proxy-login.eplug
plugin_LTLIBRARIES = libproxy-login.la

libproxy_login_la_SOURCES = proxy-login.c proxy-login.h
libproxy_login_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

libproxy_login_la_LIBADD =				\
		$(EVOLUTION_MAIL_LIBS)			\
		$(CAMEL_GROUPWISE_LIBS)


glade_DATA = proxy-login-dialog.glade

error_DATA = org-gnome-proxy-login-errors.xml
errordir = $(privdatadir)/errors

EXTRA_DIST = 					\
		$(error_DATA)			\
		$(glade_DATA) 			\
		org-gnome-proxy-login.eplug.xml	\
	     	org-gnome-proxy-login-errors.xml

CLEANFILES = $(BUILT_SOURCES)

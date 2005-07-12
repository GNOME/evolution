INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA =  org-gnome-gw-account-setup.eplug
plugin_LTLIBRARIES = liborg-gnome-gw-account-setup.la 

liborg_gnome_gw_account_setup_la_SOURCES = \
			camel-gw-listener.c	\
			camel-gw-listener.h	\
			groupwise-account-setup.c

liborg_gnome_gw_account_setup_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_gw_account_setup_la_LIBADD=			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(EVOLUTION_MAIL_LIBS)					\
	$(CAMEL_GROUPWISE_LIBS)

EXTRA_DIST = org-gnome-gw-account-setup.eplug.xml

INCLUDES = -I .						\
	-I$(top_srcdir)	                                \
	$(EVOLUTION_MAIL_CFLAGS)                        \
	$(CAMEL_GROUPWISE_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-gw-account-setup.eplug
plugin_LTLIBRARIES = liborg-gnome-gw-account-setup.la

liborg_gnome_gw_account_setup_la_SOURCES = groupwise-account-setup.c camel-gw-listener.c
liborg_gnome_gw_account_setup_la_LIBADD = $(CAMEL_GROUPWISE_LIBS)  \
					  $(top_builddir)/e-util/libeutil.la \
					  $(top_builddir)/widgets/misc/libemiscwidgets.la

liborg_gnome_gw_account_setup_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-gw-account-setup.eplug.in

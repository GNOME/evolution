INCLUDES = -I .						\
	-I$(top_srcdir)	                                \
	$(EVOLUTION_MAIL_CFLAGS) 			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-exchange-account-setup.eplug
plugin_LTLIBRARIES = liborg-gnome-exchange-account-settings.la

liborg_gnome_exchange_account_settings_la_SOURCES = 		\
	exchange-account-setup.c				\
	exchange-ask-password.c

liborg_gnome_exchange_account_settings_la_LIBADD = 		\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la

liborg_gnome_exchange_account_settings_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = org-gnome-exchange-account-setup.eplug.in

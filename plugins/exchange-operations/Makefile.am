INCLUDES = -I .						\
	-I$(top_srcdir)	                                \
	$(EVOLUTION_MAIL_CFLAGS) 			\
	$(CAMEL_EXCHANGE_CFLAGS)                        \
	$(CAMEL_CFLAGS)					\
	$(LDAP_CFLAGS)					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"          \
	-DCONNECTOR_GLADEDIR=\""$(gladedir)"\"


@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-exchange-operations.eplug
plugin_LTLIBRARIES = liborg-gnome-exchange-operations.la

liborg_gnome_exchange_operations_la_SOURCES = 		\
	exchange-operations.c					\
	exchange-config-listener.c				\
	exchange-account-setup.c				

liborg_gnome_exchange_operations_la_LIBADD = 		\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(CAMEL_LIBS)						\
	$(CAMEL_EXCHANGE_LIBS)

liborg_gnome_exchange_operations_la_LDFLAGS = -module -avoid-version

glade_DATA =							\
	exchange-change-password.glade				\
	exchange-delegates.glade				\
	exchange-folder-tree.glade

EXTRA_DIST = 							\
	org-gnome-exchange-operations.eplug.in			\
	$(glade_DATA)


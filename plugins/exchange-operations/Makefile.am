INCLUDES = -I .						\
	-I$(top_srcdir)	                                \
	$(EVOLUTION_MAIL_CFLAGS) 			\
	$(CAMEL_EXCHANGE_CFLAGS)                        \
	$(CAMEL_CFLAGS)					\
	$(LDAP_CFLAGS)					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"          \
	-DCONNECTOR_GLADEDIR=\""$(gladedir)"\"


@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-exchange-operations.eplug 	\
	org-gnome-folder-permissions.xml		\
	org-gnome-folder-subscription.xml

plugin_LTLIBRARIES = liborg-gnome-exchange-operations.la

liborg_gnome_exchange_operations_la_SOURCES = 		\
	exchange-operations.c				\
	exchange-operations.h				\
	exchange-config-listener.c			\
	exchange-config-listener.h			\
	exchange-calendar.c				\
	exchange-contacts.c				\
	exchange-change-password.c			\
	exchange-change-password.h			\
	exchange-delegates-user.c			\
	exchange-delegates-user.h			\
	exchange-delegates.c				\
	exchange-delegates.h				\
	exchange-user-dialog.c				\
	exchange-user-dialog.h				\
	exchange-folder-size-display.c			\
	exchange-folder-size-display.h			\
	exchange-account-setup.c			\
	exchange-permissions-dialog.c			\
	exchange-permissions-dialog.h			\
	exchange-folder-permission.c			\
	exchange-folder-subscription.c			\
	exchange-folder-subscription.h			\
	exchange-folder.c

liborg_gnome_exchange_operations_la_LIBADD = 		\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(CAMEL_LIBS)						\
	$(CAMEL_EXCHANGE_LIBS)

liborg_gnome_exchange_operations_la_LDFLAGS = -module -avoid-version

glade_DATA =							\
	exchange-change-password.glade				\
	exchange-delegates.glade				\
	exchange-folder-tree.glade				\
	exchange-permissions-dialog.glade			\		
	e-foreign-folder-dialog.glade		

error_DATA = org-gnome-exchange-operations.error
errordir = $(privdatadir)/errors

EXTRA_DIST = 							\
	org-gnome-exchange-operations.eplug.in			\
	org-gnome-exchange-operations.error.xml			\
	org-gnome-folder-permissions.xml			\
	org-gnome-folder-subscription.xml			\
	$(glade_DATA)						

BUILT_SOURCES = $(error_DATA)

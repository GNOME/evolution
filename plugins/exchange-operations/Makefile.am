@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-exchange-operations.eplug

plugin_LTLIBRARIES = liborg-gnome-exchange-operations.la

liborg_gnome_exchange_operations_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I .						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/composer			\
	-I$(top_builddir)/mail				\
	$(CAMEL_EXCHANGE_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

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
	exchange-folder.c				\
	exchange-mail-send-options.c			\
	exchange-send-options.c				\
	exchange-send-options.h

liborg_gnome_exchange_operations_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la 	\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(CAMEL_EXCHANGE_LIBS)					\
	$(EVOLUTION_MAIL_LIBS)

liborg_gnome_exchange_operations_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

error_DATA = org-gnome-exchange-operations.error
errordir = $(privdatadir)/errors

EXTRA_DIST = 							\
	org-gnome-exchange-operations.eplug.xml			\
	org-gnome-exchange-operations.error.xml

BUILT_SOURCES = $(error_DATA) org-gnome-exchange-operations.eplug 

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

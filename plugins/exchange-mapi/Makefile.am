INCLUDES = -I .						\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/calendar 			\
	-I$(top_builddir)/composer 			\
	-I$(top_builddir)/mail 				\
	$(CAMEL_EXCHANGE_CFLAGS) 			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\" 		\
	-DCONNECTOR_GLADEDIR=\""$(gladedir)"\" 		\
	-DLIBAMPI_LDIF_DIR=\""$(libmapi_ldif_dir)"\"	\
	$(LIBMAPI_CFLAGS)


@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-exchange-mapi.eplug 	

plugin_LTLIBRARIES = liborg-gnome-exchange-mapi.la

liborg_gnome_exchange_mapi_la_SOURCES = 		\
	exchange-mapi-account-setup.c			\
	exchange-account-listener.c

liborg_gnome_exchange_mapi_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la 	\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(top_builddir)/addressbook/gui/component/libevolution-addressbook.la \
	$(CAMEL_EXCHANGE_LIBS) \
	$(LIBMAPI_LIBS)
# ^ has to be removed

liborg_gnome_exchange_mapi_la_LDFLAGS = -module -avoid-version -lmapi $(NO_UNDEFINED)
liborg_gnome_exchange_mapi_la_CFLAGS = -I/usr/local/samba/include/

#error_DATA = org-gnome-exchange-operations.error
#errordir = $(privdatadir)/errors

EXTRA_DIST = 							\
	org-gnome-exchange-mapi.eplug.xml

BUILT_SOURCES = org-gnome-exchange-mapi.eplug	

CLEANFILES = $(BUILT_SOURCES)

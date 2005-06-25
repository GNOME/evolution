INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/camel                           \
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-groupwise-features.eplug org-gnome-compose-send-options.xml
plugin_LTLIBRARIES = liborg-gnome-groupwise-features.la

liborg_gnome_groupwise_features_la_SOURCES = \
		share-folder-common.c	     \
	       	share-folder.c 		     \
		share-folder.h               \
		install-shared.c             \
		groupwise-account-setup.c    \
		camel-gw-listener.c          \
		camel-gw-listener.h          \
		send-options.c               \
		mail-send-options.c          \
		mail-send-options.h          \
		status-track.c               \
		addressbook-groupwise.c

liborg_gnome_groupwise_features_la_LIBADD=		\
	$(CAMEL_GROUPWISE_LIBS)                        	\
	$(top_builddir)/e-util/libeutil.la 	       	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(top_builddir)/mail/libevolution-mail.la      	\
	$(top_builddir)/addressbook/gui/component/libevolution-addressbook.la



liborg_gnome_groupwise_features_la_LDFLAGS = -module -avoid-version

glade_DATA =properties.glade 

error_DATA = org-gnome-shared-folder.errors.xml
errordir = $(privdatadir)/errors

EXTRA_DIST =						\
	$(error_DATA)					\
       	$(glade_DATA) 					\
	org-gnome-compose-send-options.xml		\
	org-gnome-groupwise-features.eplug.xml
CLEANFILES = $(BUILT_SOURCES)

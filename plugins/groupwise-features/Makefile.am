if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = $(top_builddir)/mail/libevolution-mail.la
endif

INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/camel                           \
	-I$(top_builddir)/composer                      \
	-I$(top_builddir)/mail                          \
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-groupwise-features.eplug org-gnome-compose-send-options.xml
plugin_LTLIBRARIES = liborg-gnome-groupwise-features.la

liborg_gnome_groupwise_features_la_SOURCES = 	\
		share-folder-common.c	     	\
	       	share-folder.c 		     	\
		share-folder.h               	\
		install-shared.c             	\
		send-options.c               	\
		mail-send-options.c          	\
		mail-send-options.h          	\
		status-track.c               	\
		addressbook-groupwise.c      	\
		junk-mail-settings.c		\
		junk-settings.c 		\
		junk-settings.h			\
		proxy.h				\
		proxy.c				\
		proxy-login.h			\
		proxy-login.c			\
		process-meeting.c

liborg_gnome_groupwise_features_la_LIBADD=			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(NO_UNDEFINED_REQUIRED_LIBS)				\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(EVOLUTION_MAIL_LIBS)					\
	$(CAMEL_GROUPWISE_LIBS)



liborg_gnome_groupwise_features_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

glade_DATA =			\
	properties.glade	\
	junk-settings.glade 	\
	proxy-add-dialog.glade	\
	proxy-listing.glade	\
	proxy-login-dialog.glade

error_DATA = 					\
	org-gnome-shared-folder.errors.xml	\
	org-gnome-proxy-errors.xml		\
	org-gnome-proxy-login-errors.xml

errordir = $(privdatadir)/errors

BUILT_SOURCES = $(error_i18n)

EXTRA_DIST =						\
	$(error_DATA)					\
	$(glade_DATA) 					\
	org-gnome-compose-send-options.xml		\
	org-gnome-groupwise-features.eplug.xml		\
	org-gnome-proxy-login-errors.xml

CLEANFILES = $(BUILT_SOURCES)

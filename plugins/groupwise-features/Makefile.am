if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = $(top_builddir)/mail/libevolution-mail.la
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-groupwise-features.eplug org-gnome-compose-send-options.xml

plugin_LTLIBRARIES = liborg-gnome-groupwise-features.la

liborg_gnome_groupwise_features_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/composer                      \
	-I$(top_builddir)/mail                          \
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	

liborg_gnome_groupwise_features_la_SOURCES = 	\
		gw-ui.h	     			\
		gw-ui.c	     			\
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
		process-meeting.c               \
		mail-retract.c			\
		camel-gw-listener.c		\
		camel-gw-listener.h		\
		groupwise-account-setup.c

liborg_gnome_groupwise_features_la_LIBADD=			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(EVOLUTION_MAIL_LIBS)					\
	$(CAMEL_GROUPWISE_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

liborg_gnome_groupwise_features_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

ui_DATA =			\
	properties.ui	\
	junk-settings.ui 	\
	proxy-add-dialog.ui	\
	proxy-listing.ui	\
	proxy-login-dialog.ui

error_DATA = 				\
	org-gnome-shared-folder.error	\
	org-gnome-proxy.error		\
	org-gnome-proxy-login.error     \
	org-gnome-process-meeting.error \
	org-gnome-mail-retract.error

errordir = $(privdatadir)/errors

BUILT_SOURCES = org-gnome-groupwise-features.eplug $(error_DATA)

EXTRA_DIST =						\
	$(ui_DATA) 					\
	org-gnome-compose-send-options.xml		\
	org-gnome-groupwise-features.eplug.xml		\
	org-gnome-shared-folder.error.xml		\
	org-gnome-process-meeting.error.xml		\
	org-gnome-proxy.error.xml			\
	org-gnome-proxy-login.error.xml     		\
	org-gnome-mail-retract.error.xml

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = $(top_builddir)/mail/libevolution-mail.la
endif

AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/mail				\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(CAMEL_IMAP_CFLAGS)				\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-imap-features.eplug 
plugin_LTLIBRARIES = liborg-gnome-imap-features.la

liborg_gnome_imap_features_la_SOURCES = 	\
	imap-headers.c	

liborg_gnome_imap_features_la_LIBADD=		\
	$(NO_UNDEFINED_REQUIRED_LIBS)		\
	$(EVOLUTION_MAIL_LIBS)					


liborg_gnome_imap_features_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

glade_DATA =			\
	imap-headers.glade 

errordir = $(privdatadir)/errors

BUILT_SOURCES = org-gnome-imap-features.eplug $(error_i18n)

EXTRA_DIST =						\
	$(glade_DATA) 					\
	org-gnome-imap-features.eplug.xml		

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

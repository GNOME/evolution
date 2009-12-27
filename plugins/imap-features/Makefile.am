if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = $(top_builddir)/mail/libevolution-mail.la
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-imap-features.eplug

plugin_LTLIBRARIES = liborg-gnome-imap-features.la

liborg_gnome_imap_features_la_CPPFLAGS = 		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/mail				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(CAMEL_IMAP_CFLAGS)				\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"

liborg_gnome_imap_features_la_SOURCES = imap-headers.c

liborg_gnome_imap_features_la_LIBADD=			\
	$(top_builddir)/e-util/libeutil.la 		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

liborg_gnome_imap_features_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

ui_DATA =			\
	imap-headers.ui

errordir = $(privdatadir)/errors

BUILT_SOURCES = org-gnome-imap-features.eplug $(error_i18n)

EXTRA_DIST =						\
	$(ui_DATA) 					\
	org-gnome-imap-features.eplug.xml

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

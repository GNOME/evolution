@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-email-custom-header.eplug

plugin_LTLIBRARIES = liborg-gnome-email-custom-header.la

liborg_gnome_email_custom_header_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I.	 					\
	-I$(top_srcdir)					\
	-I$(top_builddir)/mail				\
	-I$(top_builddir)/composer			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_email_custom_header_la_SOURCES = 		\
	email-custom-header.c				\
	email-custom-header.h

liborg_gnome_email_custom_header_la_LIBADD = 			\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/composer/libevolution-mail-composer.la	\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

liborg_gnome_email_custom_header_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

ui_DATA =							\
	org-gnome-email-custom-header.ui

EXTRA_DIST = 							\
	org-gnome-email-custom-header.eplug.xml			\
	$(ui_DATA)					

BUILT_SOURCES = 				\
	org-gnome-email-custom-header.eplug 			\
	$(error_i18n)

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

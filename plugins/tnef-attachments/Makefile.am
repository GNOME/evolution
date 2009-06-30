if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = 				\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/mail/libevolution-mail.la
endif

AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	-DLOCALEDIR="\"$(localedir)\""			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(TNEF_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-tnef-attachments.eplug
plugin_LTLIBRARIES = liborg-gnome-tnef-attachments.la

liborg_gnome_tnef_attachments_la_SOURCES = tnef-plugin.c
liborg_gnome_tnef_attachments_la_LDFLAGS = -module -avoid-version  $(NO_UNDEFINED)
liborg_gnome_tnef_attachments_la_LIBADD = 		\
	$(NO_UNDEFINED_REQUIRED_LIBS)			\
	-lytnef

EXTRA_DIST = org-gnome-tnef-attachments.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

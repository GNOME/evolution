@EVO_PLUGIN_RULE@

error_DATA = org-gnome-attachment-reminder.error

errordir = $(privdatadir)/errors

plugin_DATA = 						\
	org-gnome-evolution-attachment-reminder.eplug

plugin_LTLIBRARIES = liborg-gnome-evolution-attachment-reminder.la

liborg_gnome_evolution_attachment_reminder_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I$(top_builddir)/composer			\
	-I$(top_srcdir)					\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

liborg_gnome_evolution_attachment_reminder_la_SOURCES = attachment-reminder.c 

liborg_gnome_evolution_attachment_reminder_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

liborg_gnome_evolution_attachment_reminder_la_LIBADD =	\
	$(top_builddir)/composer/libevolution-mail-composer.la \
	$(top_builddir)/shell/libevolution-shell.la	\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la	\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(NULL)

EXTRA_DIST = org-gnome-evolution-attachment-reminder.eplug.xml \
	org-gnome-attachment-reminder.error.xml

BUILT_SOURCES = org-gnome-evolution-attachment-reminder.eplug \
		org-gnome-attachment-reminder.error

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

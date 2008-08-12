INCLUDES =						\
	-I$(top_builddir)/composer			\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""

@EVO_PLUGIN_RULE@

error_DATA = 					\
	org-gnome-attachment-reminder.error	


errordir = $(privdatadir)/errors

plugin_DATA = 						\
	org-gnome-evolution-attachment-reminder.eplug	\
	attachment-reminder.glade

plugin_LTLIBRARIES = liborg-gnome-evolution-attachment-reminder.la

liborg_gnome_evolution_attachment_reminder_la_SOURCES = attachment-reminder.c 
liborg_gnome_evolution_attachment_reminder_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_evolution_attachment_reminder_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-attachment-reminder.schemas.in
schema_DATA     = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then                                                                                          \
		for p in $(schema_DATA) ; do                                                                                    \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p;             \
		done                                                                                                            \
	fi

EXTRA_DIST = org-gnome-evolution-attachment-reminder.eplug.xml \
	org-gnome-attachment-reminder.error.xml			\
	$(schema_in_files)					\
	attachment-reminder.glade

BUILT_SOURCES = org-gnome-evolution-attachment-reminder.eplug \
		org-gnome-attachment-reminder.error

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

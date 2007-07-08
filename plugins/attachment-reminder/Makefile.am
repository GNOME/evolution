INCLUDES =						\
	-I$(top_builddir)/composer			\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""

error_DATA = 					\
	org-gnome-attachment-reminder.errors.xml	


errordir = $(privdatadir)/errors


@EVO_PLUGIN_RULE@

plugin_DATA = 						\
	org-gnome-evolution-attachment-reminder.eplug	\
	attachment-reminder.glade

plugin_LTLIBRARIES = liborg-gnome-evolution-attachment-reminder.la

liborg_gnome_evolution_attachment_reminder_la_SOURCES = attachment-reminder.c 
liborg_gnome_evolution_attachment_reminder_la_LDFLAGS = -module -avoid-version

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-attachment-reminder.schemas.in.in
schema_DATA     = $(schema_in_files:.schemas.in.in=-$(BASE_VERSION).schemas)

%-$(BASE_VERSION).schemas.in: %.schemas.in.in
	cp $< $@

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then                                                                                          \
		for p in $(schema_DATA) ; do                                                                                    \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p;             \
		done                                                                                                            \
	fi

EXTRA_DIST = org-gnome-evolution-attachment-reminder.eplug.xml \
	$(error_DATA)					

CLEANFILES = org-gnome-evolution-attachment-reminder.eplug

DISTCLEANFILES = $(schema_DATA)

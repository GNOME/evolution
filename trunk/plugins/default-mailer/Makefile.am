INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-default-mailer.error
errordir = $(privdatadir)/errors

plugin_DATA        = org-gnome-default-mailer.eplug 
plugin_LTLIBRARIES = liborg-gnome-default-mailer.la

liborg_gnome_default_mailer_la_SOURCES = default-mailer.c
liborg_gnome_default_mailer_la_LDFLAGS = -module -avoid-version

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-mail-prompts-checkdefault.schemas.in.in
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

EXTRA_DIST = \
	org-gnome-default-mailer.eplug.xml	\
	org-gnome-default-mailer.error.xml	\
	$(schema_in_files)

BUILT_SOURCES = org-gnome-default-mailer.eplug	\
	org-gnome-default-mailer.error

CLEANFILES = $(BUILT_SOURCES)

INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-bogo-junk-plugin.eplug
plugin_LTLIBRARIES = liborg-gnome-bogo-junk-plugin.la

liborg_gnome_bogo_junk_plugin_la_SOURCES = bf-junk-filter.c
liborg_gnome_bogo_junk_plugin_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = bogo-junk-plugin.schemas.in.in
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


BUILT_SOURCES = $(plugin_DATA) $(error_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	org-gnome-bogo-junk-plugin.eplug.in



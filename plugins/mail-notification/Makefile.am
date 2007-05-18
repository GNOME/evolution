INCLUDES =						\
	-I$(top_srcdir)					\
	-DDBUS_API_SUBJECT_TO_CHANGE=1		\
	-DDBUS_VERSION=$(DBUS_VERSION)          \
	$(EVOLUTION_MAIL_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)
	$(NMN_CFLAGS)						

LIBS =							\
	$(LIBNOTIFY_LIBS)				\
	$(NMN_LIBS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-notification.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-notification.la

liborg_gnome_mail_notification_la_SOURCES = mail-notification.c
liborg_gnome_mail_notification_la_LDFLAGS = -module -avoid-version

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-mail-notification.schemas.in.in
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

EXTRA_DIST = org-gnome-mail-notification.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

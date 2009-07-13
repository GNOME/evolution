if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(GNOME_PLATFORM_LIBS)
endif

AM_CPPFLAGS =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	$(EVOLUTION_MAIL_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)

if DBUS_SUPPORT
AM_CPPFLAGS += 	-DDBUS_API_SUBJECT_TO_CHANGE=1		\
		$(DBUS_GLIB_CFLAGS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-notification.eplug
plugin_LTLIBRARIES = liborg-gnome-mail-notification.la

liborg_gnome_mail_notification_la_SOURCES = mail-notification.c
liborg_gnome_mail_notification_la_LDFLAGS = 	\
	-module -avoid-version $(NO_UNDEFINED)
liborg_gnome_mail_notification_la_LIBADD = 	\
	$(LIBNOTIFY_LIBS)			\
	$(NO_UNDEFINED_REQUIRED_LIBS)

if DBUS_SUPPORT
liborg_gnome_mail_notification_la_LIBADD += $(DBUS_GLIB_LIBS)
endif 

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-mail-notification.schemas.in
schema_DATA     = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if OS_WIN32
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE);	\
			 echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat;	\
			cmd /c _temp.bat; 						\
			rm _temp.bat;							\
		done									\
	fi
else
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) 		\
				$(GCONFTOOL) --makefile-install-rule $$p;		\
		done									\
	fi
endif

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

EXTRA_DIST = 					\
	org-gnome-mail-notification.eplug.xml	\
	$(schema_in_files)

-include $(top_srcdir)/git.mk

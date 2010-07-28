if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(GNOME_PLATFORM_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-mail-notification.eplug

plugin_LTLIBRARIES = liborg-gnome-mail-notification.la

liborg_gnome_mail_notification_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	$(GNOME_PLATFORM_CFLAGS)		\
	$(EVOLUTION_MAIL_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)			\
	$(CANBERRA_CFLAGS)

liborg_gnome_mail_notification_la_SOURCES = mail-notification.c

liborg_gnome_mail_notification_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_mail_notification_la_LIBADD = 		\
	$(CANBERRA_LIBS)				\
	$(LIBNOTIFY_LIBS)				\
	$(top_builddir)/e-util/libeutil.la 		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/shell/libeshell.la		\
	$(EVOLUTION_MAIL_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-mail-notification.schemas.in
schema_DATA     = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL

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

endif

BUILT_SOURCES = $(plugin_DATA)

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

EXTRA_DIST = 					\
	org-gnome-mail-notification.eplug.xml	\
	$(schema_in_files)

-include $(top_srcdir)/git.mk

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-email-custom-header.eplug

plugin_LTLIBRARIES = liborg-gnome-email-custom-header.la

liborg_gnome_email_custom_header_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I.	 					\
	-I$(top_srcdir)	                                \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_builddir)/mail                          \
	-I$(top_builddir)/composer			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"          \
	-DCONNECTOR_UIDIR=\""$(uidir)"\"

liborg_gnome_email_custom_header_la_SOURCES = 		\
	email-custom-header.c				\
	email-custom-header.h										

liborg_gnome_email_custom_header_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/composer/libcomposer.la 		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/mail/libevolution-mail.la 		\
	$(EVOLUTION_MAIL_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

liborg_gnome_email_custom_header_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_email_custom_header.schemas.in
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

ui_DATA =							\
	org-gnome-email-custom-header.ui

EXTRA_DIST = 							\
	org-gnome-email-custom-header.eplug.xml			\
	$(schema_in_files)					\
	$(ui_DATA)					

BUILT_SOURCES = 				\
	org-gnome-email-custom-header.eplug 			\
	$(error_i18n)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk

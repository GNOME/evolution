if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = \
	$(EVOLUTION_MAIL_LIBS) \
	$(GNOME_PLATFORM_LIBS) \
	$(top_builddir)/e-util/libeutil.la \
	$(top_builddir)/mail/libevolution-mail.la
endif

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-external-editor.error

errordir = $(privdatadir)/errors

plugin_DATA = 						\
	org-gnome-external-editor.eplug

plugin_LTLIBRARIES = liborg-gnome-external-editor.la

liborg_gnome_external_editor_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/composer			\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(SHELL_CFLAGS)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(E_UTIL_CFLAGS)				

liborg_gnome_external_editor_la_SOURCES = 		\
	external-editor.c

liborg_gnome_external_editor_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)
liborg_gnome_external_editor_la_LIBADD = 			\
	$(top_builddir)/e-util/libeutil.la 			\
	$(top_builddir)/composer/libcomposer.la 		\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la 		\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(EVOLUTION_MAIL_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-external-editor.schemas.in
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

EXTRA_DIST = 					\
	org-gnome-external-editor.eplug.xml	\
	org-gnome-external-editor.error.xml	\
	$(schema_in_files)

BUILT_SOURCES = org-gnome-external-editor.eplug \
		org-gnome-external-editor.error

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk

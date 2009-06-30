AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-default-mailer.error
errordir = $(privdatadir)/errors

plugin_DATA        = org-gnome-default-mailer.eplug 
plugin_LTLIBRARIES = liborg-gnome-default-mailer.la

liborg_gnome_default_mailer_la_SOURCES = default-mailer.c
liborg_gnome_default_mailer_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_default_mailer_la_LIBADD =	\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-mail-prompts-checkdefault.schemas.in
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

EXTRA_DIST =					\
	org-gnome-default-mailer.eplug.xml	\
	org-gnome-default-mailer.error.xml	\
	$(schema_in_files)

BUILT_SOURCES = org-gnome-default-mailer.eplug	\
	org-gnome-default-mailer.error

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk

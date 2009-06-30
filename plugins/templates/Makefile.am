AM_CPPFLAGS =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_builddir)				\
	-I$(top_builddir)/composer			\
	$(EVOLUTION_MAIL_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_PLUGINDIR="\"$(plugindir)\""

@EVO_PLUGIN_RULE@

plugin_DATA = 					\
		org-gnome-templates.eplug	\
		templates.glade

plugin_LTLIBRARIES = liborg-gnome-templates.la

liborg_gnome_templates_la_SOURCES = templates.c
liborg_gnome_templates_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_templates_la_LIBADD =	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(EVOLUTION_MAIL_LIBS)

schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps-evolution-template-placeholders.schemas.in
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

EXTRA_DIST = 	org-gnome-templates.eplug.xml	\
		$(schema_in_files)	   	\
		templates.glade

BUILT_SOURCES = org-gnome-templates.eplug

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk

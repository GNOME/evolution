module_LTLIBRARIES = libevolution-module-spamassassin.la

libevolution_module_spamassassin_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-spamassassin\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

libevolution_module_spamassassin_la_SOURCES =			\
	evolution-spamassassin.c

libevolution_module_spamassassin_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/shell/libeshell.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_spamassassin_la_LDFLAGS =			\
	-module -avoid-version $(NO_UNDEFINED)

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = evolution-spamassassin.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

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

DISTCLEANFILES = $(schema_DATA)

EXTRA_DIST = $(schema_in_files)

-include $(top_srcdir)/git.mk

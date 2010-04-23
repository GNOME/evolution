module_LTLIBRARIES = libevolution-module-addressbook.la

libevolution_module_addressbook_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"		\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/widgets/menus				\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)/addressbook/util			\
	-I$(top_srcdir)/addressbook/gui/contact-editor		\
	-I$(top_srcdir)/addressbook/gui/contact-list-editor	\
	-I$(top_srcdir)/addressbook/gui/widgets			\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"		\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DPREFIX=\""$(prefix)"\"				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)				\
	$(LDAP_CFLAGS)

libevolution_module_addressbook_la_SOURCES = \
	evolution-module-addressbook.c				\
	addressbook-config.c					\
	addressbook-config.h					\
	autocompletion-config.c					\
	autocompletion-config.h					\
	eab-composer-util.c					\
	eab-composer-util.h					\
	e-book-config-hook.c					\
	e-book-config-hook.h					\
	e-book-shell-backend.c					\
	e-book-shell-backend.h					\
	e-book-shell-content.c					\
	e-book-shell-content.h					\
	e-book-shell-migrate.c					\
	e-book-shell-migrate.h					\
	e-book-shell-settings.c					\
	e-book-shell-settings.h					\
	e-book-shell-sidebar.c					\
	e-book-shell-sidebar.h					\
	e-book-shell-view.c					\
	e-book-shell-view.h					\
	e-book-shell-view-actions.c				\
	e-book-shell-view-actions.h				\
	e-book-shell-view-private.c				\
	e-book-shell-view-private.h

if ENABLE_SMIME
SMIME_LIB=$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_module_addressbook_la_LIBADD =			\
	$(SMIME_LIB)						\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/composer/libcomposer.la			\
	$(top_builddir)/addressbook/printing/libecontactprint.la \
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/addressbook/gui/merging/libeabbookmerging.la \
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la \
	$(top_builddir)/filter/libfilter.la			\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/widgets/table/libetable.la		\
	$(top_builddir)/widgets/text/libetext.la		\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/widgets/menus/libmenus.la		\
	$(top_builddir)/addressbook/importers/libevolution-addressbook-importers.la \
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LDAP_LIBS)


libevolution_module_addressbook_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_addressbook.schemas.in
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

ui_DATA =			\
	ldap-config.ui

DISTCLEANFILES = $(schema_DATA)

EXTRA_DIST =			\
	$(ui_DATA)		\
	$(schema_in_files)	\
	openldap-extract.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-addressbook.pure

evolution-addressbook.pure: evolution-addressbook
	@rm -f evolution-addressbook.pure
	$(PLINK) $(evolution_addressbook_LDFLAGS) $(evolution_addressbook_OBJECTS) $(evolution_addressbook_LDADD) $(LIBS)

endif

-include $(top_srcdir)/git.mk

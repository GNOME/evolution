module_LTLIBRARIES = module-addressbook.la

module_addressbook_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"		\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell					\
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
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CHAMPLAIN_CFLAGS)					\
	$(GTKHTML_CFLAGS)					\
	$(LDAP_CFLAGS)

module_addressbook_la_SOURCES = \
	evolution-module-addressbook.c				\
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

module_addressbook_la_LIBADD = \
	$(SMIME_LIB)						\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/composer/libevolution-mail-composer.la	\
	$(top_builddir)/addressbook/printing/libecontactprint.la \
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la \
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/addressbook/importers/libevolution-addressbook-importers.la \
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(CHAMPLAIN_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(LDAP_LIBS)


module_addressbook_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST =			\
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

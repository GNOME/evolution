SUBDIRS	= select-names

CPPFLAGS =					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"	\
	$(EXTRA_GNOME_CFLAGS)				\
	$(GNOME_INCLUDEDIR)				\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)/widgets/e-table			\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/addressbook/gui/minicard	\
	-I$(top_srcdir)/addressbook/contact-editor	\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"

bin_PROGRAMS =	\
	evolution-addressbook

evolution_addressbook_SOURCES = 	\
	addressbook-component.c		\
	addressbook-component.h		\
	addressbook-factory.c		\
	addressbook.c			\
	addressbook.h			\
	e-addressbook-model.c		\
	e-addressbook-model.h		\
	e-cardlist-model.c		\
	e-cardlist-model.h		\
	e-ldap-server-dialog.c          \
	e-ldap-server-dialog.h		\
	e-ldap-storage.c		\
	e-ldap-storage.h

evolution_addressbook_LDADD =						\
	select-names/libeselectnames.la					\
	$(top_builddir)/shell/libeshell.a				\
	$(EXTRA_GNOME_LIBS)						\
	$(BONOBO_HTML_GNOME_LIBS)					\
	$(top_builddir)/addressbook/gui/minicard/libeminicard.a		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/addressbook/ename/libename.la			\
	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
	$(top_builddir)/libversit/libversit.la				\
	$(top_builddir)/widgets/e-text/libetext.a			\
	$(top_builddir)/widgets/e-table/libetable.a			\
	$(top_builddir)/widgets/misc/libemiscwidgets.a 			\
	$(top_builddir)/addressbook/printing/libecontactprint.a		\
	$(top_builddir)/addressbook/gui/search/libeaddressbooksearch.a	\
	$(top_builddir)/filter/libfilter.la				\
	$(top_builddir)/e-util/libeutil.la

evolution_addressbook_LDFLAGS = `gnome-config --libs gdk_pixbuf` -export-dynamic

if USING_OAF
oafdir = $(datadir)/oaf
oaf_DATA = addressbook.oafinfo
else
gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = addressbook.gnorba
endif

gladedir   = $(datadir)/evolution/glade
glade_DATA = ldap-server-dialog.glade ldap-server-dialog.glade.h alphabet.glade

EXTRA_DIST = 			\
	$(glade_DATA)		\
	addressbook.gnorba	\
	addressbook.oafinfo	\
	alphabet.glade.h

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-addressbook.pure

evolution-addressbook.pure: evolution-addressbook
	@rm -f evolution-addressbook.pure
	$(PLINK) $(evolution_addressbook_LDFLAGS) $(evolution_addressbook_OBJECTS) $(evolution_addressbook_LDADD) $(LIBS)

endif

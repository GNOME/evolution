INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"	\
	$(EXTRA_GNOME_CFLAGS)				\
	$(GNOME_INCLUDEDIR)				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets/e-text			\
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
	addressbook-factory.c		\
	addressbook.c			\
	addressbook.h

evolution_addressbook_LDADD =						\
	$(EXTRA_GNOME_LIBS)						\
	$(BONOBO_HTML_GNOME_LIBS)					\
	$(top_builddir)/addressbook/gui/minicard/libeminicard.a		\
	$(top_builddir)/widgets/e-text/libetext.a			\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/addressbook/backend/ebook/libebook.la		\
	$(top_builddir)/addressbook/contact-editor/libecontacteditor.a	\
	$(top_builddir)/libversit/libversit.la

evolution_addressbook_LDFLAGS = `gnome-config --libs gdk_pixbuf`

if USING_OAF
oafdir = $(datadir)/oaf
oaf_DATA = addressbook.oafinfo
else
gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = addressbook.gnorba
endif

EXTRA_DIST = 			\
	addressbook.gnorba	\
	addressbook.oafinfo

INCLUDES =					\
	$(EXTRA_GNOME_CFLAGS)			\
	$(GNOME_INCLUDEDIR)			\
	-I$(top_srcdir)/widgets/e-text		\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)/widgets/e-table		\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets/e-minicard	\
	-I$(top_srcdir)/addressbook/backend/ebook	\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"	\
	-DG_LOG_DOMAIN=\"addressbook-demo\"			

noinst_PROGRAMS =	\
	test-addressbook

test_addressbook_SOURCES =		\
	demo.c				\
	e-test-model.c			\
	e-test-model.h

test_addressbook_LDADD =				\
	$(EXTRA_GNOME_LIBS)			\
	$(BONOBO_HTML_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-minicard/libeminicard.a	\
	$(top_builddir)/widgets/e-table/libetable.a	\
	$(top_builddir)/widgets/e-text/libetext.a	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.la

test_addressbook_LDFLAGS = `gnome-config --libs gdk_pixbuf`

bin_PROGRAMS =	\
	evolution-addressbook

evolution_addressbook_SOURCES = 	\
	addressbook-factory.c		\
	addressbook-widget.c		\
	addressbook-widget.h		\
	addressbook.c			\
	addressbook.h			\
	e-test-model.c			\
	e-test-model.h

evolution_addressbook_LDADD =				\
	$(EXTRA_GNOME_LIBS)				\
	$(BONOBO_HTML_GNOME_LIBS)			\
	$(top_builddir)/widgets/e-minicard/libeminicard.a	\
	$(top_builddir)/widgets/e-table/libetable.a	\
	$(top_builddir)/widgets/e-text/libetext.a	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/addressbook/backend/ebook/libebook.la	\
	$(top_builddir)/libversit/libversit.la

evolution_addressbook_LDFLAGS = `gnome-config --libs gdk_pixbuf`

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = addressbook.gnorba

noinst_PROGRAMS = test-card test-client test-client-list

privlibexec_PROGRAMS =				\
	evolution-vcard-importer		\
	evolution-ldif-importer

toolsdir = $(privlibexecdir)/
tools_PROGRAMS =				\
	load-pine-addressbook			\
	load-gnomecard-addressbook

# CORBA stuff

CORBA_ADDRESSBOOK_SOURCE_H =			\
	addressbook.h
CORBA_ADDRESSBOOK_SOURCE_C =			\
	addressbook-common.c			\
	addressbook-stubs.c			\
	addressbook-skels.c
CORBA_ADDRESSBOOK_IDL = $(srcdir)/../idl/addressbook.idl

$(CORBA_ADDRESSBOOK_SOURCE_H): $(CORBA_ADDRESSBOOK_IDL)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(CORBA_ADDRESSBOOK_IDL)

$(CORBA_ADDRESSBOOK_SOURCE_C): $(CORBA_ADDRESSBOOK_SOURCE_H)

CORBA_SOURCE_H = $(CORBA_ADDRESSBOOK_SOURCE_H)
CORBA_SOURCE_C = $(CORBA_ADDRESSBOOK_SOURCE_C)
CORBA_SOURCE = $(CORBA_SOURCE_H) $(CORBA_SOURCE_C)


# The component 

INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/ename		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/ename		\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

privlib_LTLIBRARIES = libebook.la libebook-static.la

# Make sure the libraries are not built in parallel when using a parallel make;
# libtool can't handle that.
libebook_static_la_DEPENDENCIES = libebook.la

libebook_la_SOURCES =					\
	$(CORBA_SOURCE)					\
	e-book-listener.c				\
	e-book-marshal.c				\
	e-book-view-listener.c				\
	e-book-view.c					\
	e-book.c					\
	e-book-util.c					\
	e-card-cursor.c					\
	e-card-simple.c					\
	e-card.c					\
	e-card-compare.c				\
	e-destination.c

libebook_la_LIBADD =					\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/e-util/libeutil.la

libebookincludedir = $(privincludedir)/ebook

libebookinclude_HEADERS =				\
	e-book-listener.h				\
	e-book-types.h					\
	e-book-view-listener.h				\
	e-book-view.h					\
	e-book.h					\
	e-book-util.h					\
	e-card-cursor.h					\
	e-card-simple.h					\
	e-card-types.h					\
	e-card.h					\
	e-card-compare.h				\
	e-destination.h					\
	addressbook.h


#
# make a static library for use by addressbook's conduit's shared library
#
libebook_static_la_SOURCES = $(libebook_la_SOURCES)
libebook_static_la_LDFLAGS = -all-static

# GLib marshalling cruft

e-book-marshal.h: e-book-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=e_book_marshal e-book-marshal.list --header > e-book-marshal.h.tmp \
	&& mv e-book-marshal.h.tmp e-book-marshal.h ) \
	|| ( rm -f e-book-marshal.h.tmp && exit 1 )

e-book-marshal.c: e-book-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=e_book_marshal e-book-marshal.list --body > e-book-marshal.c.tmp \
	&& mv e-book-marshal.c.tmp e-book-marshal.c ) \
	|| ( rm -f e-book-marshal.c.tmp && exit 1 )

MARSHAL_GENERATED = e-book-marshal.c e-book-marshal.h

test_client_SOURCES =					\
	test-client.c

test_client_LDADD =					\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

test_client_list_SOURCES =				\
	test-client-list.c

test_client_list_LDADD =				\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

test_card_SOURCES =					\
	test-card.c

test_card_LDADD =					\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

evolution_vcard_importer_SOURCES = \
	evolution-vcard-importer.c

evolution_vcard_importer_LDADD =				\
	libebook.la						\
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(DB3_LDADD)

evolution_ldif_importer_SOURCES = \
	evolution-ldif-importer.c

evolution_ldif_importer_LDADD =					\
	libebook.la						\
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(DB3_LDADD)

load_pine_addressbook_SOURCES =				\
	load-pine-addressbook.c

load_pine_addressbook_LDADD =				\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

load_gnomecard_addressbook_SOURCES =			\
	load-gnomecard-addressbook.c

load_gnomecard_addressbook_LDADD =			\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)


BUILT_SOURCES = $(CORBA_SOURCE) $(MARSHAL_GENERATED) $(server_DATA)
CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

server_in_files = GNOME_Evolution_Addressbook_VCard_Importer.server.in.in \
		  GNOME_Evolution_Addressbook_LDIF_Importer.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(privlibexecdir)|" $< > $@


@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA) e-book-marshal.list

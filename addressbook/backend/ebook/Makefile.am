noinst_PROGRAMS = test-card test-client test-client-list

# PENDING_PORT_WORK
# bin_PROGRAMS =	evolution-vcard-importer 	\
# 		evolution-ldif-importer		\
# 		load-pine-addressbook		\
# 		load-gnomecard-addressbook

CORBA_SOURCE =				\
	addressbook.h			\
	addressbook-common.c		\
	addressbook-stubs.c		\
	addressbook-skels.c		\
        Evolution-Composer.h            \
        Evolution-Composer-common.c     \
        Evolution-Composer-skels.c      \
        Evolution-Composer-stubs.c

idls =								\
	$(srcdir)/../idl/addressbook.idl			\
	$(srcdir)/../../../composer/Evolution-Composer.idl

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES) $(idls)

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(localedir)"\"        	\
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/ename		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/ename		\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-DG_DISABLE_DEPRECATED				\
	-DLIBGNOME_DISABLE_DEPRECATED			\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

privlib_LTLIBRARIES = libebook.la libebook-static.la

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


libebookincludedir = $(includedir)/evolution/ebook

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
	( @GLIB_GENMARSHAL@ --prefix=e_book_marshal e-book-marshal.list --header > e-book-marshal.tmp \
	&& mv e-book-marshal.tmp e-book-marshal.h ) \
	|| ( rm -f e-book-marshal.tmp && exit 1 )

e-book-marshal.c: e-book-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=e_book_marshal e-book-marshal.list --body > e-book-marshal.tmp \
	&& mv e-book-marshal.tmp e-book-marshal.c ) \
	|| ( rm -f e-book-marshal.tmp && exit 1 )

MARSHAL_GENERATED = e-book-marshal.c e-book-marshal.h

test_client_SOURCES =					\
	test-client.c

test_client_LDADD =					\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)			\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/libversit/libversit.a		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/e-util/libeutil.la

test_client_list_SOURCES =				\
	test-client-list.c

test_client_list_LDADD =				\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)			\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.a		\
	$(top_builddir)/e-util/libeutil.la

test_card_SOURCES =					\
	test-card.c

test_card_LDADD =					\
	libebook.la					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)			\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.a		\
	$(top_builddir)/e-util/libeutil.la

# evolution_vcard_importer_SOURCES = \
# 	evolution-vcard-importer.c

# evolution_vcard_importer_LDADD =				\
# 	libebook.la						\
# 	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
# 	$(top_builddir)/camel/libcamel.la			\
# 	$(top_builddir)/shell/importer/libevolution-importer.la \
# 	$(DB3_LDADD)						\
# 	$(top_builddir)/e-util/ename/libename.la		\
# 	$(top_builddir)/libversit/libversit.a			\
# 	$(top_builddir)/e-util/libeutil.la

# evolution_ldif_importer_SOURCES = \
# 	evolution-ldif-importer.c

# evolution_ldif_importer_LDADD =					\
# 	libebook.la						\
# 	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
# 	$(top_builddir)/camel/libcamel.la			\
# 	$(top_builddir)/shell/importer/libevolution-importer.la \
# 	$(DB3_LDADD)						\
# 	$(top_builddir)/e-util/ename/libename.la		\
# 	$(top_builddir)/libversit/libversit.a			\
# 	$(top_builddir)/e-util/libeutil.la

# load_pine_addressbook_SOURCES =				\
# 	load-pine-addressbook.c

# load_pine_addressbook_LDADD =				\
# 	libebook.la					\
# 	$(EVOLUTION_ADDRESSBOOK_LIBS)			\
# 	$(top_builddir)/camel/libcamel.la		\
# 	$(top_builddir)/e-util/ename/libename.la	\
# 	$(top_builddir)/libversit/libversit.a		\
# 	$(top_builddir)/e-util/libeutil.la

# load_gnomecard_addressbook_SOURCES =			\
# 	load-gnomecard-addressbook.c

# load_gnomecard_addressbook_LDADD =			\
# 	libebook.la					\
# 	$(EVOLUTION_ADDRESSBOOK_LIBS)			\
# 	$(top_builddir)/camel/libcamel.la		\
# 	$(top_builddir)/e-util/ename/libename.la	\
# 	$(top_builddir)/libversit/libversit.a		\
# 	$(top_builddir)/e-util/libeutil.la


BUILT_SOURCES = $(CORBA_SOURCE) $(MARSHAL_GENERATED)
CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

serversdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Evolution_Addressbook_VCard_Importer.server.in \
		  GNOME_Evolution_Addressbook_LDIF_Importer.server.in
servers_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(servers_DATA) e-book-marshal.list

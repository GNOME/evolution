noinst_PROGRAMS = test-card test-client test-client-list load-pine-addressbook

CORBA_SOURCE =						\
	addressbook.h					\
	addressbook-common.c				\
	addressbook-stubs.c				\
	addressbook-skels.c

idls =							\
	$(srcdir)/../idl/addressbook.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) $(srcdir)/../idl/addressbook.idl $(idl_flags)

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/ename		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/ename		\
        $(BONOBO_GNOME_CFLAGS)

lib_LTLIBRARIES = libebook.la

libebook_la_SOURCES =					\
	$(CORBA_SOURCE)					\
	e-book-listener.c				\
	e-book-view-listener.c				\
	e-book-view.c					\
	e-book.c					\
	e-card-cursor.c					\
	e-card-iterator.c				\
	e-card-list-iterator.c				\
	e-card-list.c					\
	e-card-simple.c					\
	e-card.c

libebookincludedir = $(includedir)/evolution/ebook

libebookinclude_HEADERS =				\
	e-book-listener.h				\
	e-book-types.h					\
	e-book-view-listener.h				\
	e-book-view.h					\
	e-book.h					\
	e-card-cursor.h					\
	e-card-iterator.h				\
	e-card-list-iterator.h				\
	e-card-list.h					\
	e-card-pairs.h					\
	e-card-simple.h					\
	e-card-types.h					\
	e-card.h

test_client_SOURCES =					\
	test-client.c

test_client_LDADD =					\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/addressbook/ename/libename.la	\
	$(top_builddir)/e-util/libeutil.la

test_client_list_SOURCES =				\
	test-client-list.c

test_client_list_LDADD =				\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(top_builddir)/addressbook/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la

test_card_SOURCES =					\
	test-card.c

test_card_LDADD =					\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(top_builddir)/addressbook/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la


load_pine_addressbook_SOURCES =					\
	load-pine-addressbook.c

load_pine_addressbook_LDADD =					\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(top_builddir)/addressbook/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la


BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES   += $(BUILT_SOURCES)

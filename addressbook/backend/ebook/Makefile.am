bin_PROGRAMS = test-card test-client test-client-list

corbadir = $(sysconfdir)/CORBA/servers

CORBA_SOURCE =						\
	addressbook.h					\
	addressbook-common.c				\
	addressbook-stubs.c				\
	addressbook-skels.c

idls =							\
	$(srcdir)/../idl/addressbook.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl`

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) $(srcdir)/../idl/addressbook.idl $(idl_flags)

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(builddir)					\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(srcdir)/.					\
	-I$(srcdir)/..					\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)

gnome_libs =						\
	$(GNOME_LIBDIR)					\
        $(GNOMEUI_LIBS)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)

ebook_libs = 						\
	libebook.la 					\
	$(gnome_libs)

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
	e-card.c

libebookincludedir = $(includedir)/backend

libebookinclude_HEADERS =				\
	e-book-listener.h				\
	e-book-view-listener.h				\
	e-book-view.h					\
	e-book.h					\
	e-card-cursor.h					\
	e-card-iterator.h				\
	e-card-list-iterator.h				\
	e-card-list.h					\
	e-card.h

test_client_SOURCES =					\
	test-client.c

test_client_LDADD =					\
	$(GTK_LIBS)					\
	$(GNOME_LIBDIR)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)					\
	-lbonobo					\
	$(ebook_libs)					\
	$(top_builddir)/libversit/libversit.la

test_client_list_SOURCES =				\
	test-client-list.c

test_client_list_LDADD =				\
	$(GTK_LIBS)					\
	$(GNOME_LIBDIR)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)					\
	-lbonobo					\
	$(ebook_libs)					\
	$(top_builddir)/libversit/libversit.la

test_card_SOURCES =					\
	test-card.c

test_card_LDADD =					\
	$(GTK_LIBS)					\
	$(GNOME_LIBDIR)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)					\
	-lbonobo					\
	$(ebook_libs)					\
	$(top_builddir)/libversit/libversit.la

BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES   += $(BUILT_SOURCES)

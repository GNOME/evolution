# CORBA stuff

SUBDIRS = . tests

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
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/ename		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/ename		\
	-DG_DISABLE_DEPRECATED				\
	-DLIBGNOME_DISABLE_DEPRECATED			\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

privlib_LTLIBRARIES = libebook.la

libebook_la_SOURCES =					\
	$(CORBA_SOURCE)					\
	addressbook.h					\
	e-book-marshal.c				\
	e-book-query.c					\
	e-book-view-listener.c				\
	e-book-view.c					\
	e-book-listener.c				\
	e-book.c					\
	e-book-async.c					\
	e-contact.c					\
	e-vcard.c

libebook_la_LIBADD =					\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/e-util/libeutil.la

libebookincludedir = $(privincludedir)/ebook

libebookinclude_HEADERS =				\
	e-book-query.h					\
	e-book-view-listener.h				\
	e-book-view.h					\
	e-book-types.h					\
	e-book-listener.h				\
	e-book.h					\
	e-contact.h					\
	e-vcard.h

MARSHAL_GENERATED = e-book-marshal.c e-book-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(CORBA_SOURCE) $(MARSHAL_GENERATED)
CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

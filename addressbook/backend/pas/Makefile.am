lib_LTLIBRARIES = libpas.la

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
	-DG_LOG_DOMAIN=\"Wombat\"			\
	-I$(builddir)					\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(srcdir)/. 					\
	-I$(srcdir)/.. 					\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
	-I$(top_srcdir)/e-util				\
	-I$(top_srcdir)/addressbook/backend/ebook	\
        $(GNOME_INCLUDEDIR)

gnome_libs =						\
	$(GNOME_LIBDIR)					\
        $(GNOMEUI_LIBS)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)

if ENABLE_LDAP
LDAP_BACKEND = pas-backend-ldap.c
endif

libpas_la_SOURCES =					\
	$(CORBA_SOURCE)					\
	pas-book-factory.c				\
	pas-book-view.c					\
	pas-book.c					\
	pas-backend-file.c				\
	$(LDAP_BACKEND)					\
	pas-backend.c					\
	pas-card-cursor.c

libpasincludedir = $(includedir)/backend

libpasinclude_HEADERS =					\
	pas-book-factory.h				\
	pas-book-view.h					\
	pas-book.h					\
	pas-backend-file.h				\
	pas-backend.h					\
	pas-card-cursor.h

BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES   += $(BUILT_SOURCES)

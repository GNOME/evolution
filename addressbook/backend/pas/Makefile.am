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
	-DG_LOG_DOMAIN=\"wombat-pas\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
        $(BONOBO_GNOME_CFLAGS)

LDAP_BACKEND_SOURCES = pas-backend-ldap.c pas-backend-ldap.h

if ENABLE_LDAP
LDAP_BACKEND = $(LDAP_BACKEND_SOURCES)
else
LDAP_BACKEND = 
endif

noinst_LIBRARIES = libpas.a

libpas_a_SOURCES =					\
	$(CORBA_SOURCE)					\
	pas-book-factory.c				\
	pas-book-factory.h				\
	pas-book-view.c					\
	pas-book-view.h					\
	pas-book.c					\
	pas-book.h					\
	pas-backend-file.c				\
	pas-backend-file.h				\
	$(LDAP_BACKEND)					\
	pas-backend.c					\
	pas-backend.h					\
	pas-card-cursor.c				\
	pas-card-cursor.h

BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES   += $(BUILT_SOURCES)

EXTRA_DIST = $(LDAP_BACKEND_SOURCES)
CORBA_SOURCE_H = addressbook.h

CORBA_SOURCE_C =					\
	addressbook-common.c				\
	addressbook-stubs.c				\
	addressbook-skels.c

CORBA_SOURCE = $(CORBA_SOURCE_H) $(CORBA_SOURCE_C)

idls =							\
	$(srcdir)/../idl/addressbook.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) -I $(srcdir) $(srcdir)/../idl/addressbook.idl $(idl_flags)

INCLUDES =						\
	$(DB3_CFLAGS)					\
	$(LDAP_CFLAGS)					\
        -DGNOMELOCALEDIR=\""$(localedir)"\"		\
	-DG_LOG_DOMAIN=\"wombat-pas\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

LDAP_SCHEMA =						\
	evolutionperson.schema

LDAP_BACKEND_FILES_C = pas-backend-ldap.c
LDAP_BACKEND_FILES_H = pas-backend-ldap.h

if ENABLE_LDAP
LDAP_BACKEND_C = $(LDAP_BACKEND_FILES_C)
LDAP_BACKEND_H = $(LDAP_BACKEND_FILES_H)
else
LDAP_BACKEND_C = 
LDAP_BACKEND_H =
endif

privlib_LIBRARIES = libpas.a

pasincludedir = $(includedir)/evolution/pas

pasinclude_HEADERS =					\
	$(CORBA_SOURCE_H)				\
	pas-book-factory.h				\
	pas-book-view.h					\
	pas-book.h					\
	pas-backend-card-sexp.h				\
	pas-backend-file.h				\
	$(LDAP_BACKEND_H)				\
	pas-backend.h					\
	pas-backend-summary.h				\
	pas-card-cursor.h

libpas_a_SOURCES =					\
	$(pasinclude_HEADERS)				\
	$(CORBA_SOURCE_C)				\
	pas-book-factory.c				\
	pas-book-view.c					\
	pas-book.c					\
	pas-backend-card-sexp.c				\
	pas-backend-file.c				\
	$(LDAP_BACKEND_C)				\
	pas-backend.c					\
	pas-backend-summary.c				\
	pas-card-cursor.c

BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

EXTRA_DIST = $(LDAP_BACKEND_FILES_C) $(LDAP_BACKEND_FILES_H) $(LDAP_SCHEMA)

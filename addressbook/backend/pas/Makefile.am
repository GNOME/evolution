CORBA_SOURCE_H = addressbook.h

CORBA_SOURCE_C =					\
	addressbook-common.c				\
	addressbook-stubs.c				\
	addressbook-skels.c

CORBA_SOURCE = $(CORBA_SOURCE_H) $(CORBA_SOURCE_C)

idls =							\
	$(srcdir)/../idl/addressbook.idl

$(CORBA_SOURCE_H): $(idls)
	$(ORBIT_IDL)  $(IDL_INCLUDES) -I $(srcdir) $(srcdir)/../idl/addressbook.idl

$(CORBA_SOURCE_C): $(CORBA_SOURCE_H)


INCLUDES =						\
	$(DB3_CFLAGS)					\
	$(LDAP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"wombat-pas\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/backend		\
	-DG_DISABLE_DEPRECATED				\
	-DLIBGNOME_DISABLE_DEPRECATED			\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

LDAP_SCHEMA =						\
	evolutionperson.schema

LDAP_BACKEND_FILES =					\
	pas-backend-ldap.c				\
	pas-backend-ldap.h

if ENABLE_LDAP
LDAP_BACKEND = libpasldap.a
endif

ldapschemadir = $(privdatadir)
ldapschema_DATA= $(LDAP_SCHEMA)

privlib_LIBRARIES = libpas.a
noinst_LIBRARIES = libpasfile.a $(LDAP_BACKEND)

pasincludedir = $(privincludedir)/pas

pasinclude_HEADERS =					\
	$(CORBA_SOURCE_H)				\
	pas-book-factory.h				\
	pas-book-view.h					\
	pas-book.h					\
	pas-backend-card-sexp.h				\
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
	pas-backend.c					\
	pas-backend-summary.c				\
	pas-card-cursor.c				\
	pas-marshal.c

libpasfile_a_SOURCES =					\
	pas-backend-file.c				\
	pas-backend-file.h

if ENABLE_LDAP
libpasldap_a_SOURCES =					\
	$(LDAP_BACKEND_FILES)
endif

MARSHAL_GENERATED = pas-marshal.c pas-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(CORBA_SOURCE) $(MARSHAL_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

EXTRA_DIST = $(LDAP_BACKEND_FILES) $(LDAP_SCHEMA) pas-marshal.list

noinst_PROGRAMS = test-card test-client test-client-list

bin_PROGRAMS = evolution-vcard-importer 		\
	load-pine-addressbook load-gnomecard-addressbook

CORBA_SOURCE =						\
	addressbook.h					\
	addressbook-common.c				\
	addressbook-stubs.c				\
	addressbook-skels.c				\
        Evolution-Composer.h              \
        Evolution-Composer-common.c       \
        Evolution-Composer-skels.c        \
        Evolution-Composer-stubs.c

idls =							\
	$(srcdir)/../idl/addressbook.idl		\
	$(srcdir)/../../../composer/Evolution-Composer.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) -I $(srcdir) $(idls) $(idl_flags)

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(localedir)"\"        \
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/addressbook/backend		\
	-I$(top_srcdir)/addressbook/ename		\
	-I$(top_builddir)/addressbook/backend		\
	-I$(top_builddir)/addressbook/ename		\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
        $(BONOBO_GNOME_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)

lib_LTLIBRARIES = libebook.la

libebook_la_SOURCES =					\
	$(CORBA_SOURCE)					\
	e-book-listener.c				\
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
	e-destination.h


#
# make a static library for use by addressbook's conduit's shared library
#
noinst_LTLIBRARIES = libebook-static.la
libebook_static_la_SOURCES = $(libebook_la_SOURCES)
libebook_static_la_LDFLAGS = --all-static


test_client_SOURCES =					\
	test-client.c

test_client_LDADD =					\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)				\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/e-util/libeutil.la

test_client_list_SOURCES =				\
	test-client-list.c

test_client_list_LDADD =				\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)				\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la

test_card_SOURCES =					\
	test-card.c

test_card_LDADD =					\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)				\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la

evolution_vcard_importer_SOURCES =			\
	evolution-vcard-importer.c
evolution_vcard_importer_LDADD =				\
	libebook.la						\
	$(BONOBO_GNOME_LIBS)					\
	$(EXTRA_GNOME_LIBS)					\
	$(top_builddir)/shell/importer/libevolution-importer.la \
	$(DB3_LDADD)						\
	$(top_builddir)/e-util/ename/libename.la		\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/e-util/libeutil.la

load_pine_addressbook_SOURCES =				\
	load-pine-addressbook.c

load_pine_addressbook_LDADD =				\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)				\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la


load_gnomecard_addressbook_SOURCES =			\
	load-gnomecard-addressbook.c

load_gnomecard_addressbook_LDADD =			\
	libebook.la					\
	$(BONOBO_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)				\
	$(top_builddir)/e-util/ename/libename.la	\
	$(top_builddir)/libversit/libversit.la		\
	$(top_builddir)/e-util/libeutil.la


BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES   += $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_Evolution_Addressbook_VCard_Importer.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

# add this back when you add the file to the archive ;)
#	GNOME_Evolution_Addressbook_Pine_Importer.oafinfo

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)





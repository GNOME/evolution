bin_PROGRAMS = wombat
lib_LTLIBRARIES = libpas.la

corbadir = $(sysconfdir)/CORBA/servers

CORBA_SOURCE =						\
	addressbook.h					\
	addressbook-common.c				\
	addressbook-stubs.c				\
	addressbook-skels.c

idls =							\
	../idl/addressbook.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl`

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) ../idl/addressbook.idl $(idl_flags)

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Wombat\"			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)

gnome_libs =						\
	$(GNOME_LIBDIR)					\
        $(GNOMEUI_LIBS)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)

pas_libs = 						\
	libpas.la 					\
	$(gnome_libs)


libpas_la_SOURCES =					\
	$(CORBA_SOURCE)					\
	pas-book-factory.c				\
	pas-book-view.c					\
	pas-book.c					\
	pas-backend-file.c				\
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

wombat_SOURCES =					\
	pas.c

wombat_LDADD =						\
	$(GTK_LIBS)					\
	$(GNOME_LIBDIR)					\
	$(GNOMEGNORBA_LIBS)				\
	$(INTLLIBS)					\
	-lbonobo					\
	$(pas_libs)

BUILT_SOURCES = $(CORBA_SOURCE)
CLEANFILES   += $(BUILT_SOURCES)

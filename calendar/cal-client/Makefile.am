#
# libcal-client
#

CORBA_GENERATED =				\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

CORBA_HEADERS_GENERATED =			\
	evolution-calendar.h

idls =						\
	$(srcdir)/../idl/evolution-calendar.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(srcdir)/../idl/evolution-calendar.idl

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(localedir)"\"		\
	-DG_LOG_DOMAIN=\"cal-client\"			\
	-I$(top_srcdir)/calendar			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/libical/src/libical		\
	$(BONOBO_GNOME_CFLAGS)

lib_LTLIBRARIES = libcal-client.la

libcal_clientincludedir = $(includedir)/evolution/cal-client

libcal_client_la_SOURCES =	\
	$(CORBA_GENERATED)	\
	cal-client-types.c	\
	cal-client.c		\
	cal-listener.c		\
	cal-listener.h		\
	cal-query.c		\
	query-listener.c	\
	query-listener.h

libcal_clientinclude_HEADERS =		\
	$(CORBA_HEADERS_GENERATED)	\
	cal-client-types.h 		\
	cal-client.h			\
	cal-query.h

#
# make a static library for use by calendar conduit's shared library
#
noinst_LTLIBRARIES = libcal-client-static.la
libcal_client_static_la_SOURCES = $(libcal_client_la_SOURCES)
libcal_client_static_la_LDFLAGS = --all-static


#
# client-test program
#

noinst_PROGRAMS = client-test

client_test_SOURCES =	\
	client-test.c

client_test_INCLUDES =			\
	$(INCLUDES)			\
	-DG_LOG_DOMAIN=\"client-test\"

client_test_LDADD =						\
	$(BONOBO_VFS_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)					\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/libical/src/libical/libical.la		\
	libcal-client.la

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES += $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

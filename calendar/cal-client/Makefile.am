#
# libcal-client
#

CORBA_GENERATED_C =				\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c
CORBA_GENERATED_H =			\
	evolution-calendar.h

CORBA_GENERATED = $(CORBA_GENERATED_C) $(CORBA_GENERATED_H)

idls =						\
	$(srcdir)/../idl/evolution-calendar.idl

idl_flags = $(IDL_INCLUDES)

$(CORBA_GENERATED_H): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(srcdir)/../idl/evolution-calendar.idl
$(CORBA_GENERATED_C): $(CORBA_GENERATED_H)

INCLUDES =						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DG_LOG_DOMAIN=\"cal-client\"			\
	-I$(top_srcdir)/calendar			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/libical/src/libical		\
	$(EVOLUTION_CALENDAR_CFLAGS)

privlib_LTLIBRARIES = libcal-client.la

libcal_clientincludedir = $(privincludedir)/cal-client

libcal_client_la_SOURCES =	\
	$(CORBA_GENERATED_C)	\
	cal-client-types.c	\
	cal-client.c		\
	cal-listener.c		\
	cal-listener.h		\
	cal-query.c		\
	query-listener.c	\
	query-listener.h

libcal_client_la_LIBADD =					\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/e-util/libeutil.la

libcal_clientinclude_HEADERS =		\
	$(CORBA_GENERATED_H)		\
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

client_test_LDADD =			\
	libcal-client.la		\
	$(EVOLUTION_CALENDAR_LIBS)

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

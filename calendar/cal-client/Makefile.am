#
# libcal-client
#

CORBA_GENERATED =				\
	evolution-calendar.h			\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

idls =						\
	$(srcdir)/../idl/evolution-calendar.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl`

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(srcdir)/../idl/evolution-calendar.idl

INCLUDES =						\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"cal-client\"			\
	-I$(top_srcdir)/calendar/cal-util		\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
	-I.. 						\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)

lib_LTLIBRARIES = libcal-client.la

libcal_clientincludedir = $(includedir)/cal-client

libcal_client_la_SOURCES =	\
	$(CORBA_GENERATED)	\
	cal-client.c		\
	cal-listener.c		\
	cal-listener.h

libcal_clientinclude_HEADERS =	\
	cal-client.h


#
# client-test program
#

noinst_PROGRAMS = client-test

client_test_SOURCES =	\
	client-test.c

client_test_INCLUDES =			\
	$(INCLUDES)			\
	-DG_LOG_DOMAIN=\"client-test\"

client_test_LDADD =					\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(top_builddir)/libversit/libversit.la		\
	$(top_srcdir)/calendar/cal-util/libcalutil.la	\
	libcal-client.la

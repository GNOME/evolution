INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat-pcs\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	-I$(top_srcdir)/libical/src			\
	-I$(top_builddir)/libical/src			\
	-I$(top_srcdir)/libwombat			\
	-I$(top_builddir)/libwombat			\
	$(EVOLUTION_CALENDAR_CFLAGS)

AM_CFLAGS = 								\
	-DGTK_DISABLE_DEPRECATED=1					\
	-DGDK_DISABLE_DEPRECATED=1					\
	-DG_DISABLE_DEPRECATED=1					\
	-DGNOME_DISABLE_DEPRECATED=1

CORBA_GENERATED_H =				\
	evolution-calendar.h

CORBA_GENERATED_C =				\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

CORBA_GENERATED = $(CORBA_GENERATED_H) $(CORBA_GENERATED_C)

idls =						\
	$(srcdir)/../idl/evolution-calendar.idl

idl_flags = -I $(srcdir) $(IDL_INCLUDES)

$(CORBA_GENERATED_H): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(srcdir)/../idl/evolution-calendar.idl

$(CORBA_GENERATED_C): $(CORBA_GENERATED_H)

pcsincludedir = $(privincludedir)/pcs

privlib_LTLIBRARIES = libpcs.la
noinst_LTLIBRARIES = libpcsfile.la

pcsinclude_HEADERS = 		\
	$(CORBA_GENERATED_H)	\
	cal.h			\
	cal-backend.h		\
	cal-backend-sync.h	\
	cal-backend-util.h	\
	cal-backend-object-sexp.h\
	cal-common.h		\
	cal-factory.h		\
	query.h

libpcs_la_SOURCES =		\
	$(pcsinclude_HEADERS)	\
	$(CORBA_GENERATED_C)	\
	cal.c			\
	cal-backend.c		\
	cal-backend-sync.c	\
	cal-backend-util.c	\
	cal-backend-object-sexp.c\
	cal-factory.c		\
	query.c

libpcs_la_LIBADD =						\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/e-util/libeutil.la

libpcsfile_la_SOURCES =		\
	cal-backend-file-events.c\
	cal-backend-file-events.h\
	cal-backend-file-todos.c\
	cal-backend-file-todos.h\
	cal-backend-file.c	\
	cal-backend-file.h

libpcsfile_la_LIBADD =		\
	libpcs.la

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)


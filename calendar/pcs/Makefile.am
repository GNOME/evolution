INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat-pcs\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	-I$(top_srcdir)/libical/src			\
	-I$(top_srcdir)/libwombat			\
	-I$(top_builddir)/libwombat			\
	$(EVOLUTION_CALENDAR_CFLAGS)

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

privlib_LIBRARIES = libpcs.a
noinst_LIBRARIES = libpcsfile.a

pcsinclude_HEADERS = 		\
	$(CORBA_GENERATED_H)	\
	cal.h			\
	cal-backend.h		\
	cal-backend-util.h	\
	cal-common.h		\
	cal-factory.h		\
	job.h			\
	query-backend.h		\
	query.h

libpcs_a_SOURCES =		\
	$(pcsinclude_HEADERS)	\
	$(CORBA_GENERATED_C)	\
	cal.c			\
	cal-backend.c		\
	cal-backend-util.c	\
	cal-factory.c		\
	job.c			\
	query-backend.c		\
	query.c

libpcsfile_a_SOURCES =		\
	cal-backend-file.c	\
	cal-backend-file.h

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)


INCLUDES =						\
	-DG_LOG_DOMAIN=\"wombat-pcs\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"

CORBA_GENERATED =				\
	evolution-calendar.h			\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

idls =						\
	$(srcdir)/../idl/evolution-calendar.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(srcdir)/../idl/evolution-calendar.idl

noinst_LIBRARIES = libpcs.a

libpcs_a_SOURCES =		\
	$(CORBA_GENERATED)	\
	cal.c			\
	cal.h			\
	cal-backend.c		\
	cal-backend.h		\
	cal-backend-imc.c	\
	cal-backend-imc.h	\
	cal-common.h		\
	cal-factory.c		\
	cal-factory.h		\
	job.c			\
	job.h


BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES += $(BUILT_SOURCES)

#
# Tlacuache personal calendar server
#

bin_PROGRAMS = tlacuache

#INCLUDES = \
#        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
#	-I$(top_srcdir)/libical/src/libical \
#	-I$(top_srcdir)/libversit \
#	-I$(top_srcdir)/calendar \
#	-I. \
#	-I$(top_srcdir)/calendar/cal-client \
#        $(GNOME_INCLUDEDIR)

INCLUDES = \
	-I$(top_srcdir)/calendar/cal-client \
	-I$(top_srcdir)/calendar/cal-util \
	-I$(top_srcdir)/calendar \
	-I$(top_srcdir)/libical/src/libical \
	$(GNOME_INCLUDEDIR) \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"

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

#tlacuache_INCLUDES =					\
#        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
#	-I$(srcdir) -I$(top_srcdir) 			\
#	-I. 						\
#	-I.. 						\
#	-I$(top_builddir)				\
#        -I$(includedir)                                 \
#        $(GNOME_INCLUDEDIR)

tlacuache_SOURCES =		\
	$(CORBA_GENERATED)	\
	cal.c			\
	cal.h			\
	cal-backend.c		\
	cal-backend.h		\
	cal-common.h		\
	cal-factory.c		\
	cal-factory.h		\
	icalendar.c		\
	icalendar.h		\
	job.c			\
	job.h			\
	tlacuache.c

#	alarm.c			\
#	alarm.h			\
#	cal-util.c		\
#	cal-util.h		\


ICAL_LINK_FLAGS = $(top_builddir)/libical/src/libical/libical.la

tlacuache_LDADD =					\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(top_srcdir)/calendar/cal-util/libcalutil.la	\
	$(top_srcdir)/libversit/libversit.la		\
	$(ICAL_LINK_FLAGS)

gnorbadir = $(sysconfdir)/CORBA/servers

gnorba_DATA =			\
	tlacuache.gnorba

EXTRA_DIST =			\
	$(gnorba_DATA)

CORBA_GENERATED =				\
	evolution-calendar.h			\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

idls = $(top_srcdir)/calendar/idl/evolution-calendar.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(top_srcdir)/calendar/idl/evolution-calendar.idl

bin_PROGRAMS = evolution-alarm-notify

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"evolution-alarm-notify\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/widgets				\
	-I$(includedir)					\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"

gladedir   = $(datadir)/evolution/glade

evolution_alarm_notify_SOURCES =	\
	notify-main.c

evolution_alarm_notify_LDADD =								\
	$(top_builddir)/calendar/cal-client/libcal-client.la				\
	$(top_builddir)/calendar/cal-util/libcal-util.la				\
	$(top_builddir)/libical/src/libical/libical.la					\
	$(BONOBO_VFS_GNOME_LIBS)							\
	$(EXTRA_GNOME_LIBS)								\
	$(INTLLIBS)

oafdir = $(datadir)/oaf
oaf_DATA = GNOME_Evolution_Calendar_AlarmNotify.oafinfo

EXTRA_DIST =		\
	$(oaf_DATA)

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES += $(BUILT_SOURCES)

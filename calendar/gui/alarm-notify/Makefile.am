CORBA_GENERATED =				\
	evolution-calendar.h			\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

idls = $(top_srcdir)/calendar/idl/evolution-calendar.idl

idl_flags = $(IDL_INCLUDES) 

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(top_srcdir)/calendar/idl/evolution-calendar.idl

evolibexecdir = $(libexecdir)/evolution/$(BASE_VERSION)
evolibexec_PROGRAMS = evolution-alarm-notify

noinst_LIBRARIES = libalarm.a

libalarm_a_SOURCES =	\
	alarm.c		\
	alarm.h

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"evolution-alarm-notify\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/widgets				\
	-I$(includedir)					\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	$(EVOLUTION_CALENDAR_CFLAGS)

gladedir   = $(datadir)/evolution-$(BASE_VERSION)/glade

glade_DATA =		\
	alarm-notify.glade

evolution_alarm_notify_SOURCES =	\
	$(CORBA_GENERATED)		\
	alarm-notify.c			\
	alarm-notify.h			\
	alarm-notify-dialog.c		\
	alarm-notify-dialog.h		\
	alarm-queue.c			\
	alarm-queue.h			\
	config-data.c			\
	config-data.h			\
	notify-main.c			\
	save.c				\
	save.h

evolution_alarm_notify_LDADD =						\
	libalarm.a							\
	$(top_builddir)/calendar/cal-client/libcal-client.la		\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(top_builddir)/libical/src/libical/libical-evolution.la	\
	$(top_builddir)/libwombat/libwombat.la				\
	$(top_builddir)/e-util/libeutil.la				\
	$(EVOLUTION_CALENDAR_LIBS)

evolution_alarm_notify_LDFLAGS = -export-dynamic

serversdir = $(libdir)/bonobo/servers
servers_in_files = \
	GNOME_Evolution_Calendar_AlarmNotify.server.in

servers_DATA = $(servers_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST =		\
	$(servers_DATA)	\
	$(servers_in_files)	\
	$(glade_DATA)

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

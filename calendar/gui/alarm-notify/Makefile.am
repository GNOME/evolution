CORBA_GENERATED =				\
	evolution-calendar.h			\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

idls = $(top_srcdir)/calendar/idl/evolution-calendar.idl

idl_flags = $(IDL_INCLUDES) 

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(top_srcdir)/calendar/idl/evolution-calendar.idl

privlibexec_PROGRAMS = evolution-alarm-notify

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
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	$(EVOLUTION_CALENDAR_CFLAGS)

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

server_in_files = \
	GNOME_Evolution_Calendar_AlarmNotify.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(privlibexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST =		\
	$(server_DATA)	\
	$(server_in_files)	\
	$(glade_DATA)

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

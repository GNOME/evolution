CORBA_GENERATED_H =				\
	evolution-calendar.h
CORBA_GENERATED_C =				\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c
CORBA_GENERATED = $(CORBA_GENERATED_C) $(CORBA_GENERATED_H)

idls = $(top_srcdir)/calendar/idl/evolution-calendar.idl
idl_flags = $(IDL_INCLUDES) 

$(CORBA_GENERATED_H): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(top_srcdir)/calendar/idl/evolution-calendar.idl
$(CORBA_GENERATED_C): $(CORBA_GENERATED_H)


privlibexec_PROGRAMS = evolution-alarm-notify


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
	alarm.c				\
	alarm.h				\
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
	$(top_builddir)/calendar/cal-client/libcal-client.la		\
	$(top_builddir)/calendar/cal-util/libcal-util.la		\
	$(EVOLUTION_CALENDAR_LIBS)

server_in_files = \
	GNOME_Evolution_Calendar_AlarmNotify.server.in.in

server_DATA = $(server_in_files:.server.in.in=.server)
%.server.in: %.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(privlibexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST =		\
	$(server_DATA)	\
	$(server_in_files)	\
	$(glade_DATA)

BUILT_SOURCES = $(CORBA_GENERATED) $(server_DATA)
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
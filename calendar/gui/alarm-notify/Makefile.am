CORBA_GENERATED =				\
        Evolution-Composer.h              		\
        Evolution-Composer-common.c       		\
        Evolution-Composer-skels.c        		\
        Evolution-Composer-stubs.c			\
	evolution-calendar.h			\
	evolution-calendar-common.c		\
	evolution-calendar-skels.c		\
	evolution-calendar-stubs.c

idls = 								\
	$(top_srcdir)/composer/Evolution-Composer.idl		\
	$(top_srcdir)/calendar/idl/evolution-calendar.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -I $(datadir)/idl

$(CORBA_GENERATED): $(idls)
	$(ORBIT_IDL) $(idl_flags) $(srcdir)/../../../composer/Evolution-Composer.idl
	$(ORBIT_IDL) $(idl_flags) $(top_srcdir)/calendar/idl/evolution-calendar.idl

bin_PROGRAMS = evolution-alarm-notify

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

iconsdir = $(datadir)/images/evolution

gladedir   = $(datadir)/evolution/glade

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

oafdir = $(datadir)/oaf
oaf_in_files = \
	GNOME_Evolution_Calendar_AlarmNotify.oaf.in

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST =		\
	$(oaf_DATA)	\
	$(oaf_in_files)	\
	$(glade_DATA)

BUILT_SOURCES = $(CORBA_GENERATED)
CLEANFILES = $(BUILT_SOURCES)

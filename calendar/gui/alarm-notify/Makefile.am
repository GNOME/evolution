if OS_WIN32
bin_PROGRAMS = evolution-alarm-notify
else
privlibexec_PROGRAMS = evolution-alarm-notify
endif

if HAVE_WINDRES
EVOLUTIONALARMNOTIFYICON = evolution-alarm-notify-icon.o
endif

evolution_alarm_notify_CPPFLAGS = 			\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"evolution-alarm-notify\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/calendar			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_ICONDIR=\""$(icondir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_LIBEXECDIR=\""$(privlibexecdir)"\"	\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(CAMEL_CFLAGS)					\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(CANBERRA_CFLAGS)

ui_DATA =		\
	alarm-notify.ui

evolution_alarm_notify_SOURCES =	\
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
	util.c				\
	util.h

evolution_alarm_notify_LDADD =						\
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/calendar/common/libevolution-calendarprivate.la	\
	$(CAMEL_LIBS)							\
	$(EVOLUTION_CALENDAR_LIBS)					\
	$(CANBERRA_LIBS)						\
	$(GNOME_PLATFORM_LIBS)						\
	$(EVOLUTIONALARMNOTIFYICON)

if OS_WIN32
evolution_alarm_notify_LDFLAGS = -mwindows
endif

EXTRA_DIST = $(ui_DATA) \
	evolution-alarm-notify-icon.rc			\
	evolution-alarm-notify.ico


evolution-alarm-notify-icon.o: evolution-alarm-notify.ico evolution-alarm-notify-icon.rc
	$(WINDRES) evolution-alarm-notify-icon.rc evolution-alarm-notify-icon.o

-include $(top_srcdir)/git.mk

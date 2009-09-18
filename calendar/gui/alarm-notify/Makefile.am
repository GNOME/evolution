bin_PROGRAMS = evolution-alarm-notify

AM_CPPFLAGS = 						\
	-DG_LOG_DOMAIN=\"evolution-alarm-notify\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/calendar			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_LIBEXECDIR=\""$(privlibexecdir)"\"	\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	$(CANBERRA_CFLAGS)

glade_DATA =		\
	alarm-notify.glade

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
	$(CANBERRA_LIBS)

if OS_WIN32
evolution_alarm_notify_LDFLAGS = -mwindows
endif

EXTRA_DIST = $(glade_DATA)

-include $(top_srcdir)/git.mk

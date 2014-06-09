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
	-I$(top_srcdir)/calendar			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_ICONDIR=\""$(icondir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_LIBEXECDIR=\""$(privlibexecdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(LIBNOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

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
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/composer/libevolution-mail-composer.la		\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(top_builddir)/calendar/gui/libevolution-calendar.la		\
	$(top_builddir)/calendar/importers/libevolution-calendar-importers.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(LIBNOTIFY_LIBS)						\
	$(CANBERRA_LIBS)						\
	$(EVOLUTIONALARMNOTIFYICON)					\
	$(NULL)

evolution_alarm_notify_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

if OS_WIN32
evolution_alarm_notify_LDFLAGS += -mwindows
endif

EXTRA_DIST = $(ui_DATA) \
	evolution-alarm-notify-icon.rc			\
	evolution-alarm-notify.ico


evolution-alarm-notify-icon.o: evolution-alarm-notify.ico evolution-alarm-notify-icon.rc
	$(WINDRES) evolution-alarm-notify-icon.rc evolution-alarm-notify-icon.o

-include $(top_srcdir)/git.mk

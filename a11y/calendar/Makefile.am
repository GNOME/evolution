# Calendar IDL files

# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

if OS_WIN32
WIN32_BOOTSTRAP_LIBS = 					\
	$(top_builddir)/win32/libetext.la		\
	$(top_builddir)/win32/libevolution-calendar.la
endif

privsolib_LTLIBRARIES = libevolution-calendar-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar/gui			\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/a11y				\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_calendar_a11y_la_SOURCES =		\
	ea-calendar.c				\
	ea-calendar.h				\
	ea-calendar-helpers.c			\
	ea-calendar-helpers.h			\
	ea-cal-view.c				\
	ea-cal-view.h				\
	ea-cal-view-event.c			\
	ea-cal-view-event.h			\
	ea-day-view.c				\
	ea-day-view.h				\
	ea-day-view-main-item.c			\
	ea-day-view-main-item.h			\
	ea-day-view-cell.c			\
	ea-day-view-cell.h			\
	ea-week-view.c				\
	ea-week-view.h				\
	ea-week-view-main-item.c		\
	ea-week-view-main-item.h		\
	ea-week-view-cell.c			\
	ea-week-view-cell.h			\
        ea-jump-button.c                        \
        ea-jump-button.h                        \
	ea-gnome-calendar.c			\
	ea-gnome-calendar.h

libevolution_calendar_a11y_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_calendar_a11y_la_LIBADD =			\
	$(WIN32_BOOTSTRAP_LIBS)				\
        $(top_builddir)/a11y/libevolution-a11y.la	\
	$(EVOLUTION_CALENDAR_LIBS)

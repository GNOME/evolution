# Calendar IDL files

# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

CALENDAR_IDLS = $(top_srcdir)/calendar/idl/evolution-calendar.idl
SHELL_IDLS = \
        $(top_srcdir)/shell/Evolution.idl

CALENDAR_IDL_GENERATED_H = evolution-calendar.h
CALENDAR_IDL_GENERATED_C =                              \
        evolution-calendar-common.c                     \
        evolution-calendar-skels.c                      \
        evolution-calendar-stubs.c

SHELL_IDL_GENERATED_H = Evolution.h
SHELL_IDL_GENERATED_C =			\
	Evolution-common.c  		\
	Evolution-skels.c 		\
	Evolution-stubs.c

$(CALENDAR_IDL_GENERATED_H): $(CALENDAR_IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)   \
                $(top_srcdir)/calendar/idl/evolution-calendar.idl

$(SHELL_IDL_GENERATED_H): $(SHELL_IDLS)
	$(ORBIT_IDL) -I $(srcdir) $(IDL_INCLUDES)  \
        $(top_srcdir)/shell/Evolution.idl

IDL_GENERATED = $(SHELL_IDL_GENERATED_H) $(CALENDAR_IDL_GENERATED_H)
BUILT_SOURCES = $(IDL_GENERATED)
CLEANFILES = $(BUILT_SOURCES) $(CALENDAR_IDL_GENERATED_C) $(SHELL_IDL_GENERATED_C)

privlib_LTLIBRARIES = libevolution-calendar-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar/cal-client		\
	-I$(top_srcdir)/calendar/gui			\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/misc			\
	-I$(top_srcdir)/a11y				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(A11Y_CFLAGS)					\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_calendar_a11y_la_SOURCES =		\
	$(CALENDAR_IDL_GENERATED_H)		\
	$(SHELL_IDL_GENERATED_H)		\
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

libevolution_calendar_a11y_la_LIBADD =   \
        $(top_builddir)/a11y/libevolution-a11y.la


# Calendar IDL files
                                                                                                                       
CALENDAR_IDLS = $(top_srcdir)/calendar/idl/evolution-calendar.idl
SHELL_IDLS = \
        $(top_srcdir)/shell/Evolution-Activity.idl                  \
        $(top_srcdir)/shell/Evolution-ConfigControl.idl             \
        $(top_srcdir)/shell/Evolution-Offline.idl                   \
        $(top_srcdir)/shell/Evolution-Session.idl                   \
        $(top_srcdir)/shell/Evolution-Shell.idl                     \
        $(top_srcdir)/shell/Evolution-ShellComponent.idl            \
        $(top_srcdir)/shell/Evolution-ShellComponentDnd.idl         \
        $(top_srcdir)/shell/Evolution-ShellView.idl                 \
        $(top_srcdir)/shell/Evolution-Shortcuts.idl                 \
        $(top_srcdir)/shell/Evolution-Storage.idl                   \
        $(top_srcdir)/shell/Evolution-StorageSetView.idl            \
        $(top_srcdir)/shell/Evolution-Wizard.idl                    \
        $(top_srcdir)/shell/Evolution-common.idl                    \
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
                                                                                                                       
noinst_LTLIBRARIES = libevolution-calendar-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar/cal-client		\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_srcdir)/calendar/gui			\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/a11y				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DG_DISABLE_DEPRECATED				\
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
	ea-week-view.c				\
	ea-week-view.h				\
	ea-gnome-calendar.c			\
	ea-gnome-calendar.h

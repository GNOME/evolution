## CORBA stuff

IDLS =                          \
        $(srcdir)/../../composer/Evolution-Composer.idl

IDL_GENERATED =                 \
        Evolution-Composer.h              \
        Evolution-Composer-common.c       \
        Evolution-Composer-skels.c        \
        Evolution-Composer-stubs.c

$(IDL_GENERATED): $(IDLS) 
	$(ORBIT_IDL) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl`   \
                -I . $(srcdir)/../../composer/Evolution-Composer.idl

##


SUBDIRS = dialogs

help_base = $(datadir)/gnome/help/cal

bin_PROGRAMS = evolution-calendar

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(top_srcdir)/widgets				\
	-I$(includedir)					\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
        -DGNOMELOCALEDIR=\""$(localedir)"\"

LINK_FLAGS =							\
	$(BONOBO_VFS_GNOME_LIBS)				\
	$(EXTRA_GNOME_LIBS)					\
	$(INTLLIBS)						\
	dialogs/libcal-dialogs.a				\
	$(top_builddir)/calendar/cal-client/libcal-client.la	\
	$(top_builddir)/calendar/cal-util/libcal-util.la	\
	$(top_builddir)/widgets/misc/libemiscwidgets.a		\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/libversit/libversit.la			\
	$(top_builddir)/libical/src/libical/libical.a		\
	$(top_builddir)/widgets/meeting-time-sel/libevolutionmtsel.a

gladedir   = $(datadir)/evolution/glade

glade_DATA = 					\
	event-editor-dialog.glade		\
	e-meeting-dialog.glade			\
	e-itip-control.glade

glade_messages = event-editor-dialog.glade.h

evolution_calendar_SOURCES =			\
	$(IDL_GENERATED)			\
	alarm.c					\
	alarm.h					\
	calendar-config.c			\
	calendar-config.h			\
	calendar-commands.c			\
	calendar-commands.h			\
	calendar-model.c			\
	calendar-model.h			\
	control-factory.c			\
	control-factory.h			\
	component-factory.c			\
	component-factory.h			\
	e-calendar-table.h			\
	e-calendar-table.c			\
	e-day-view-main-item.c			\
	e-day-view-main-item.h			\
	e-day-view-time-item.c			\
	e-day-view-time-item.h			\
	e-day-view-top-item.c			\
	e-day-view-top-item.h			\
	e-day-view.c				\
	e-day-view.h				\
	e-week-view-event-item.c		\
	e-week-view-event-item.h		\
	e-week-view-main-item.c			\
	e-week-view-main-item.h			\
	e-week-view-titles-item.c		\
	e-week-view-titles-item.h		\
	e-week-view.c				\
	e-week-view.h				\
	e-meeting-edit.h			\
	e-meeting-edit.c			\
	e-itip-control.h			\
	e-itip-control.c			\
	event-editor.c				\
	event-editor.h				\
	event-editor-dialog.glade.h		\
	getdate.y				\
	gncal-todo.c				\
	gncal-todo.h				\
	gnome-cal.c				\
	gnome-cal.h				\
	gnome-month-item.c			\
	gnome-month-item.h			\
	goto.c					\
	main.c					\
	mark.c					\
	mark.h					\
	popup-menu.c				\
	popup-menu.h				\
	print.c					\
	print.h					\
	prop.c

evolution_calendar_LDADD =					\
	$(top_builddir)/shell/libeshell.a			\
	$(EXTRA_GNOME_LIBS)					\
	$(BONOBO_HTML_GNOME_LIBS)				\
	$(top_builddir)/e-util/libeutil.la			\
	$(LINK_FLAGS)

evolution_calendar_LDFLAGS = `gnome-config --libs gdk_pixbuf`

oafdir = $(datadir)/oaf
oaf_DATA = calendar-control.oafinfo evolution-calendar.oafinfo 

EXTRA_DIST =	 			\
	$(glade_DATA)			\
	$(glade_messages)		\
	bell.xpm			\
	check-filled.xpm		\
	dayview.xpm			\
	jump.xpm			\
	monthview.xpm			\
	recur.xpm			\
	task-assigned-to.xpm		\
	task-assigned.xpm		\
	task-recurring.xpm		\
	task.xpm			\
	weekview.xpm			\
	workweekview.xpm		\
	yearview.xpm			\
	calendar-control.oafinfo	\
	evolution-calendar.oafinfo


install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(help_base)/C
	$(mkinstalldirs) $(Conduitsdir)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-calendar.pure

evolution-calendar.pure: evolution-calendar
	@rm -f evolution-calendar.pure
	$(PLINK) $(evolution_calendar_LDFLAGS) $(evolution_calendar_OBJECTS) $(evolution_calendar_LDADD) $(LIBS)

endif

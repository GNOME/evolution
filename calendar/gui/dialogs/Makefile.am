gladedir = $(datadir)/evolution/glade

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/calendar/cal-client		\
	-I$(top_builddir)/calendar/cal-client		\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(includedir)					\
	$(EXTRA_GNOME_CFLAGS)				\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
        -DGNOMELOCALEDIR=\""$(localedir)"\"

noinst_LIBRARIES = libcal-dialogs.a

libcal_dialogs_a_SOURCES =	\
	alarm-notify-dialog.c	\
	alarm-notify-dialog.h	\
	alarm-page.c		\
	alarm-page.h		\
	cal-prefs-dialog.c	\
	cal-prefs-dialog.h	\
	cancel-comp.c		\
	cancel-comp.h		\
	comp-editor.c		\
	comp-editor.h		\
	comp-editor-page.c	\
	comp-editor-page.h	\
	comp-editor-util.c	\
	comp-editor-util.h	\
	delete-comp.c		\
	delete-comp.h		\
	e-timezone-dialog.c	\
	e-timezone-dialog.h	\
	event-editor.c		\
	event-editor.h		\
	event-page.c		\
	event-page.h		\
	meeting-page.c		\
	meeting-page.h		\
	recurrence-page.c	\
	recurrence-page.h	\
	save-comp.c		\
	save-comp.h		\
	send-comp.c		\
	send-comp.h		\
	task-editor.c		\
	task-editor.h		\
	task-details-page.c	\
	task-details-page.h	\
	task-page.c		\
	task-page.h

glade_DATA =				\
	alarm-notify.glade		\
	alarm-page.glade		\
	cal-prefs-dialog.glade		\
	e-timezone-dialog.glade		\
	event-page.glade		\
	meeting-page.glade		\
	recurrence-page.glade		\
	task-details-page.glade		\
	task-page.glade

EXTRA_DIST =			\
	$(glade_DATA)

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
	delete-comp.c		\
	delete-comp.h		\
	editor-page.c		\
	editor-page.h		\
	event-page.c		\
	event-page.h		\
	save-comp.c		\
	save-comp.h		\
	task-editor.c		\
	task-editor.h

glade_DATA =				\
	alarm-notify.glade		\
	alarm-page.glade		\
	cal-prefs-dialog.glade		\
	event-page.glade		\
	recurrence-page.glade		\
	task-details-page.glade		\
	task-editor-dialog.glade	\
	task-page.glade

EXTRA_DIST =			\
	$(glade_DATA)

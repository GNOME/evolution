gladedir = $(datadir)/evolution/glade

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(includedir)					\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
        -DGNOMELOCALEDIR=\""$(localedir)"\"

noinst_LIBRARIES = libcal-dialogs.a

libcal_dialogs_a_SOURCES =	\
	alarm-notify-dialog.c	\
	alarm-notify-dialog.h	\
	cal-prefs-dialog.c	\
	cal-prefs-dialog.h	\
	delete-comp.c		\
	delete-comp.h		\
	save-comp.c		\
	save-comp.h		\
	task-editor.c		\
	task-editor.h

glade_DATA =			\
	alarm-notify.glade	\
	cal-prefs-dialog.glade	\
	task-editor-dialog.glade

glade_messages =			\
	alarm-notify.glade.h		\
	cal-prefs-dialog.glade.h	\
	task-editor-dialog.glade.h

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(glade_messages)

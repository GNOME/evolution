gladedir = $(datadir)/evolution/glade

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-I$(top_srcdir)/libical/src/libical		\
	-I$(top_builddir)/libical/src/libical		\
	-I$(includedir)					\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	$(BONOBO_VFS_GNOME_CFLAGS)			\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"

noinst_LIBRARIES = libcal-dialogs.a

libcal_dialogs_a_SOURCES =	\
	alarm-notify-dialog.c	\
	alarm-notify-dialog.h	\
	task-editor.c		\
	task-editor.h

glade_DATA =			\
	alarm-notify.glade	\
	task-editor-dialog.glade

glade_messages =		\
	alarm-notify.glade.h	\
	task-editor-dialog.glade.h

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(glade_messages)

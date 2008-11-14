INCLUDES =						\
	-DG_LOG_DOMAIN=\"calendar-modules\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	$(EVOLUTION_CALENDAR_CFLAGS)

#module_LTLIBRARIES =					\
#	libevolution-module-memos.la			\
#	libevolution-module-tasks.la

module_LTLIBRARIES =					\
	libevolution-module-calendars.la		\
	libevolution-module-memos.la			\
	libevolution-module-tasks.la

libevolution_module_calendars_la_SOURCES =		\
	e-cal-shell-module.c				\
	e-cal-shell-module-migrate.c			\
	e-cal-shell-module-migrate.h			\
	e-cal-shell-content.c				\
	e-cal-shell-content.h				\
	e-cal-shell-sidebar.c				\
	e-cal-shell-sidebar.h				\
	e-cal-shell-view.c				\
	e-cal-shell-view.h				\
	e-cal-shell-view-actions.c			\
	e-cal-shell-view-actions.h			\
	e-cal-shell-view-memopad.c			\
	e-cal-shell-view-private.c			\
	e-cal-shell-view-private.h			\
	e-cal-shell-view-taskpad.c

libevolution_module_memos_la_SOURCES =			\
	e-memo-shell-module.c				\
	e-memo-shell-module-migrate.c			\
	e-memo-shell-module-migrate.h			\
	e-memo-shell-content.c				\
	e-memo-shell-content.h				\
	e-memo-shell-sidebar.c				\
	e-memo-shell-sidebar.h				\
	e-memo-shell-view.c				\
	e-memo-shell-view.h				\
	e-memo-shell-view-actions.c			\
	e-memo-shell-view-actions.h			\
	e-memo-shell-view-private.c			\
	e-memo-shell-view-private.h

libevolution_module_tasks_la_SOURCES =			\
	e-task-shell-module.c				\
	e-task-shell-module-migrate.c			\
	e-task-shell-module-migrate.h			\
	e-task-shell-content.c				\
	e-task-shell-content.h				\
	e-task-shell-sidebar.c				\
	e-task-shell-sidebar.h				\
	e-task-shell-view.c				\
	e-task-shell-view.h				\
	e-task-shell-view-actions.c			\
	e-task-shell-view-actions.h			\
	e-task-shell-view-private.c			\
	e-task-shell-view-private.h

# Removed from all three
#	$(top_builddir)/a11y/calendar/libevolution-calendar-a11y.la

libevolution_module_calendars_la_LIBADD =		\
	$(WIN32_BOOTSTRAP_LIBS)				\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/calendar/gui/libcal-gui.la	\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la

libevolution_module_memos_la_LIBADD =			\
	$(WIN32_BOOTSTRAP_LIBS)				\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/calendar/gui/libcal-gui.la

libevolution_module_tasks_la_LIBADD =			\
	$(WIN32_BOOTSTRAP_LIBS)				\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/calendar/gui/libcal-gui.la

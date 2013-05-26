module_LTLIBRARIES = module-calendar.la

module_calendar_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"calendar-modules\"		\
	-DEVOLUTION_BINDIR=\""$(bindir)"\"		\
	-DEVOLUTION_PRIVLIBEXECDIR=\""$(PRIVLIBEXECDIR)"\" \
	-I$(top_srcdir)					\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

module_calendar_la_SOURCES = \
	evolution-module-calendar.c			\
	e-calendar-preferences.c			\
	e-calendar-preferences.h			\
	e-cal-attachment-handler.c			\
	e-cal-attachment-handler.h			\
	e-cal-config-hook.c				\
	e-cal-config-hook.h				\
	e-cal-event-hook.c				\
	e-cal-event-hook.h				\
	e-cal-shell-backend.c				\
	e-cal-shell-backend.h				\
	e-cal-shell-content.c				\
	e-cal-shell-content.h				\
	e-cal-shell-migrate.c				\
	e-cal-shell-migrate.h				\
	e-cal-shell-sidebar.c				\
	e-cal-shell-sidebar.h				\
	e-cal-shell-view.c				\
	e-cal-shell-view.h				\
	e-cal-shell-view-actions.c			\
	e-cal-shell-view-actions.h			\
	e-cal-shell-view-memopad.c			\
	e-cal-shell-view-private.c			\
	e-cal-shell-view-private.h			\
	e-cal-shell-view-taskpad.c			\
	e-memo-shell-backend.c				\
	e-memo-shell-backend.h				\
	e-memo-shell-content.c				\
	e-memo-shell-content.h				\
	e-memo-shell-migrate.c				\
	e-memo-shell-migrate.h				\
	e-memo-shell-sidebar.c				\
	e-memo-shell-sidebar.h				\
	e-memo-shell-view.c				\
	e-memo-shell-view.h				\
	e-memo-shell-view-actions.c			\
	e-memo-shell-view-actions.h			\
	e-memo-shell-view-private.c			\
	e-memo-shell-view-private.h			\
	e-task-shell-backend.c				\
	e-task-shell-backend.h				\
	e-task-shell-content.c				\
	e-task-shell-content.h				\
	e-task-shell-migrate.c				\
	e-task-shell-migrate.h				\
	e-task-shell-sidebar.c				\
	e-task-shell-sidebar.h				\
	e-task-shell-view.c				\
	e-task-shell-view.h				\
	e-task-shell-view-actions.c			\
	e-task-shell-view-actions.h			\
	e-task-shell-view-private.c			\
	e-task-shell-view-private.h

module_calendar_la_LIBADD = \
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/calendar/importers/libevolution-calendar-importers.la	\
	$(top_builddir)/mail/libevolution-mail.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la	\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/e-util/libevolution-util.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(GTKHTML_LIBS)

module_calendar_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

ui_DATA = e-calendar-preferences.ui

EXTRA_DIST =			\
	$(ui_DATA)

-include $(top_srcdir)/git.mk

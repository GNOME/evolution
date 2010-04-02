module_LTLIBRARIES =					\
	libevolution-module-calendar.la

libevolution_module_calendar_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"calendar-modules\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	$(CAMEL_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)

libevolution_module_calendar_la_SOURCES =		\
	evolution-module-calendar.c			\
	e-cal-attachment-handler.c			\
	e-cal-attachment-handler.h			\
	e-cal-config-calendar-item.c			\
	e-cal-config-calendar-item.h			\
	e-cal-config-date-edit.c			\
	e-cal-config-date-edit.h			\
	e-cal-config-hook.c				\
	e-cal-config-hook.h				\
	e-cal-config-meeting-store.c			\
	e-cal-config-meeting-store.h			\
	e-cal-config-meeting-time-selector.c		\
	e-cal-config-meeting-time-selector.h		\
	e-cal-config-model.c				\
	e-cal-config-model.h				\
	e-cal-config-view.c				\
	e-cal-config-view.h				\
	e-cal-event-hook.c				\
	e-cal-event-hook.h				\
	e-cal-shell-backend.c				\
	e-cal-shell-backend.h				\
	e-cal-shell-content.c				\
	e-cal-shell-content.h				\
	e-cal-shell-migrate.c				\
	e-cal-shell-migrate.h				\
	e-cal-shell-settings.c				\
	e-cal-shell-settings.h				\
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

libevolution_module_calendar_la_LIBADD =		\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(top_builddir)/calendar/importers/libevolution-calendar-importers.la	\
	$(top_builddir)/mail/libevolution-mail.la \
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la	\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/filter/libfilter.la				\
	$(top_builddir)/widgets/menus/libmenus.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la			\
	$(top_builddir)/widgets/table/libetable.la			\
	$(CAMEL_LIBS)							\
	$(EVOLUTION_CALENDAR_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

libevolution_module_calendar_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

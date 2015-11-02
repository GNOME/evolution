privsolib_LTLIBRARIES = libevolution-calendar.la

ecalendarincludedir = $(privincludedir)/calendar/gui

ecalendarinclude_HEADERS =			\
	calendar-config.h			\
	calendar-config-keys.h			\
	calendar-view.h				\
	comp-util.h				\
	e-alarm-list.h				\
	e-cal-config.h				\
	e-cal-data-model.h			\
	e-cal-data-model-subscriber.h		\
	e-cal-dialogs.h				\
	e-cal-event.h				\
	e-cal-list-view.h			\
	e-cal-model-calendar.h			\
	e-cal-model.h				\
	e-cal-ops.h				\
	e-calendar-view.h			\
	e-cell-date-edit-text.h			\
	e-comp-editor.h				\
	e-comp-editor-event.h			\
	e-comp-editor-memo.h			\
	e-comp-editor-page-attachments.h	\
	e-comp-editor-page-general.h		\
	e-comp-editor-page-recurrence.h		\
	e-comp-editor-page-reminders.h		\
	e-comp-editor-page-schedule.h		\
	e-comp-editor-page.h			\
	e-comp-editor-property-part.h		\
	e-comp-editor-property-parts.h		\
	e-comp-editor-task.h			\
	e-date-time-list.h                      \
	e-day-view-layout.h			\
	e-day-view-main-item.h			\
	e-day-view-time-item.h			\
	e-day-view-top-item.h			\
	e-day-view.h				\
	e-meeting-attendee.h			\
	e-meeting-list-view.h			\
	e-meeting-store.h			\
	e-meeting-time-sel.h			\
	e-meeting-time-sel-item.h		\
	e-meeting-types.h			\
	e-meeting-utils.h			\
	e-select-names-editable.h		\
	e-select-names-renderer.h		\
	e-send-options-utils.h			\
	e-week-view-event-item.h		\
	e-week-view-layout.h			\
	e-week-view-main-item.h			\
	e-week-view-titles-item.h		\
	e-week-view.h				\
	e-weekday-chooser.h			\
	e-timezone-entry.h			\
	itip-utils.h				\
	misc.h					\
	tag-calendar.h

search_files = tasktypes.xml memotypes.xml caltypes.xml

ruledir = $(privdatadir)
rule_DATA = $(search_files)

libevolution_calendar_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"calendar-gui\"			\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/calendar			\
	-DEVOLUTION_RULEDIR=\"$(ruledir)\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-DEVOLUTION_PRIVDATADIR=\""${privdatadir}"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(LIBSOUP_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

etspec_DATA =				\
	e-calendar-table.etspec		\
	e-meeting-time-sel.etspec	\
	e-cal-list-view.etspec		\
	e-memo-table.etspec

libevolution_calendar_la_SOURCES = \
	calendar-config.c			\
	calendar-config.h			\
	calendar-config-keys.h			\
	calendar-view.c				\
	calendar-view.h				\
	comp-util.c				\
	comp-util.h				\
	e-alarm-list.c				\
	e-alarm-list.h				\
	e-cal-component-preview.c		\
	e-cal-component-preview.h		\
	e-cal-config.c				\
	e-cal-config.h				\
	e-cal-data-model.c			\
	e-cal-data-model.h			\
	e-cal-data-model-subscriber.c		\
	e-cal-data-model-subscriber.h		\
	e-cal-dialogs.c				\
	e-cal-event.c				\
	e-cal-event.h				\
	e-cal-list-view.c			\
	e-cal-list-view.h			\
	e-cal-model-calendar.c			\
	e-cal-model-calendar.h			\
	e-cal-model.c				\
	e-cal-model.h				\
	e-cal-model-memos.c			\
	e-cal-model-memos.h			\
	e-cal-model-tasks.c			\
	e-cal-model-tasks.h			\
	e-cal-ops.h				\
	e-cal-ops.c				\
	e-calendar-view.c			\
	e-calendar-view.h			\
	e-cell-date-edit-text.h			\
	e-cell-date-edit-text.c			\
	e-comp-editor.c				\
	e-comp-editor-event.c			\
	e-comp-editor-memo.c			\
	e-comp-editor-page-attachments.c	\
	e-comp-editor-page-general.c		\
	e-comp-editor-page-recurrence.c		\
	e-comp-editor-page-reminders.c		\
	e-comp-editor-page-schedule.c		\
	e-comp-editor-page.c			\
	e-comp-editor-property-part.c		\
	e-comp-editor-property-parts.c		\
	e-comp-editor-task.c			\
	e-date-time-list.c			\
	e-date-time-list.h			\
	e-day-view-layout.c			\
	e-day-view-layout.h			\
	e-day-view-main-item.c			\
	e-day-view-main-item.h			\
	e-day-view-time-item.c			\
	e-day-view-time-item.h			\
	e-day-view-top-item.c			\
	e-day-view-top-item.h			\
	e-day-view.c				\
	e-day-view.h				\
	e-meeting-attendee.c			\
	e-meeting-attendee.h			\
	e-meeting-list-view.c			\
	e-meeting-list-view.h			\
	e-meeting-store.c			\
	e-meeting-store.h			\
	e-meeting-time-sel.c			\
	e-meeting-time-sel.h			\
	e-meeting-time-sel-item.c		\
	e-meeting-time-sel-item.h		\
	e-meeting-types.h			\
	e-meeting-utils.c			\
	e-meeting-utils.h			\
	e-memo-table.c				\
	e-memo-table.h				\
	e-month-view.c				\
	e-month-view.h				\
	e-select-names-editable.c		\
	e-select-names-editable.h		\
	e-select-names-renderer.c		\
	e-select-names-renderer.h		\
	e-send-options-utils.c			\
	e-task-table.c				\
	e-task-table.h				\
	e-week-view-event-item.c		\
	e-week-view-event-item.h		\
	e-week-view-layout.c			\
	e-week-view-layout.h			\
	e-week-view-main-item.c			\
	e-week-view-main-item.h			\
	e-week-view-titles-item.c		\
	e-week-view-titles-item.h		\
	e-week-view.c				\
	e-week-view.h				\
	e-weekday-chooser.c			\
	e-weekday-chooser.h			\
	e-timezone-entry.c			\
	e-timezone-entry.h			\
	itip-utils.c				\
	itip-utils.h				\
	misc.c					\
	misc.h					\
	print.c					\
	print.h					\
	tag-calendar.c				\
	tag-calendar.h				\
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
	ea-jump-button.c			\
	ea-jump-button.h

libevolution_calendar_la_LIBADD =					\
	$(top_builddir)/composer/libevolution-mail-composer.la		\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la \
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la \
	$(top_builddir)/shell/libevolution-shell.la			\
	$(top_builddir)/calendar/importers/libevolution-calendar-importers.la \
	$(top_builddir)/e-util/libevolution-util.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(LIBSOUP_LIBS)							\
	$(NULL)

libevolution_calendar_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

EXTRA_DIST =	 			\
	$(ui_DATA)			\
	$(etspec_DATA)			\
	$(search_files) 

-include $(top_srcdir)/git.mk

add_custom_xml_files(calendar ${privdatadir} filter .in ""
	tasktypes.xml
	memotypes.xml
	caltypes.xml
)

install(FILES	e-meeting-time-sel.etspec
		e-cal-list-view.etspec
		e-memo-table.etspec
		e-task-table.etspec
	DESTINATION ${etspecdir}
)


set(DEPENDENCIES
	eabutil
	econtacteditor
	econtactlisteditor
	email-engine
	evolution-calendar-importers
	evolution-mail-composer
	evolution-shell
	evolution-util
	gnomecanvas
)

set(SOURCES
	calendar-config.c
	calendar-view.c
	comp-util.c
	e-alarm-list.c
	e-bulk-edit-tasks.c
	e-cal-component-preview.c
	e-cal-component-widget.c
	e-cal-config.c
	e-cal-data-model.c
	e-cal-data-model-subscriber.c
	e-cal-day-column.c
	e-cal-dialogs.c
	e-cal-event.c
	e-cal-list-view.c
	e-cal-model.c
	e-cal-model-calendar.c
	e-cal-model-memos.c
	e-cal-model-tasks.c
	e-cal-ops.c
	e-cal-range-model.c
	e-calendar-view.c
	e-cell-date-edit-text.c
	e-cell-estimated-duration.c
	e-comp-editor.c
	e-comp-editor-event.c
	e-comp-editor-memo.c
	e-comp-editor-page-attachments.c
	e-comp-editor-page-general.c
	e-comp-editor-page-recurrence.c
	e-comp-editor-page-reminders.c
	e-comp-editor-page-schedule.c
	e-comp-editor-page.c
	e-comp-editor-property-part.c
	e-comp-editor-property-parts.c
	e-comp-editor-task.c
	e-date-time-list.c
	e-day-view.c
	e-day-view-layout.c
	e-day-view-main-item.c
	e-day-view-time-item.c
	e-day-view-top-item.c
	e-estimated-duration-entry.c
	e-meeting-attendee.c
	e-meeting-list-view.c
	e-meeting-store.c
	e-meeting-time-sel.c
	e-meeting-time-sel-item.c
	e-meeting-utils.c
	e-memo-table.c
	e-month-view.c
	e-select-names-editable.c
	e-select-names-renderer.c
	e-task-table.c
	e-to-do-pane.c
	e-week-view.c
	e-week-view-event-item.c
	e-week-view-layout.c
	e-week-view-main-item.c
	e-week-view-titles-item.c
	e-weekday-chooser.c
	e-timezone-entry.c
	e-year-view.c
	itip-utils.c
	print.c
	tag-calendar.c
	ea-calendar.c
	ea-calendar-helpers.c
	ea-cal-view.c
	ea-cal-view-event.c
	ea-day-view.c
	ea-day-view-main-item.c
	ea-day-view-cell.c
	ea-week-view.c
	ea-week-view-main-item.c
	ea-week-view-cell.c
	ea-jump-button.c
)

set(HEADERS
	calendar-config.h
	calendar-config-keys.h
	calendar-view.h
	comp-util.h
	e-alarm-list.h
	e-bulk-edit-tasks.h
	e-cal-component-preview.h
	e-cal-component-widget.h
	e-cal-config.h
	e-cal-data-model.h
	e-cal-data-model-subscriber.h
	e-cal-day-column.h
	e-cal-dialogs.h
	e-cal-event.h
	e-cal-list-view.h
	e-cal-model-calendar.h
	e-cal-model.h
	e-cal-model-memos.h
	e-cal-model-tasks.h
	e-cal-ops.h
	e-cal-range-model.h
	e-calendar-view.h
	e-cell-date-edit-text.h
	e-cell-estimated-duration.h
	e-comp-editor.h
	e-comp-editor-event.h
	e-comp-editor-memo.h
	e-comp-editor-page-attachments.h
	e-comp-editor-page-general.h
	e-comp-editor-page-recurrence.h
	e-comp-editor-page-reminders.h
	e-comp-editor-page-schedule.h
	e-comp-editor-page.h
	e-comp-editor-property-part.h
	e-comp-editor-property-parts.h
	e-comp-editor-task.h
	e-estimated-duration-entry.h
	e-date-time-list.h
	e-day-view-layout.h
	e-day-view-main-item.h
	e-day-view-time-item.h
	e-day-view-top-item.h
	e-day-view.h
	e-meeting-attendee.h
	e-meeting-list-view.h
	e-meeting-store.h
	e-meeting-time-sel.h
	e-meeting-time-sel-item.h
	e-meeting-types.h
	e-meeting-utils.h
	e-memo-table.h
	e-month-view.h
	e-select-names-editable.h
	e-select-names-renderer.h
	e-task-table.h
	e-to-do-pane.h
	e-week-view-event-item.h
	e-week-view-layout.h
	e-week-view-main-item.h
	e-week-view-titles-item.h
	e-week-view.h
	e-weekday-chooser.h
	e-timezone-entry.h
	e-year-view.h
	itip-utils.h
	print.h
	tag-calendar.h
	ea-calendar.h
	ea-calendar-helpers.h
	ea-cal-view.h
	ea-cal-view-event.h
	ea-day-view.h
	ea-day-view-main-item.h
	ea-day-view-cell.h
	ea-week-view.h
	ea-week-view-main-item.h
	ea-week-view-cell.h
	ea-jump-button.h
)

add_library(evolution-calendar SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(evolution-calendar
	${DEPENDENCIES}
)

target_compile_definitions(evolution-calendar PRIVATE
	-DG_LOG_DOMAIN=\"evolution-calendar\"
	-DEVOLUTION_ETSPECDIR=\"${etspecdir}\"
)

target_compile_options(evolution-calendar PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${LIBSOUP_CFLAGS}
)

target_include_directories(evolution-calendar PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${LIBSOUP_INCLUDE_DIRS}
)

target_link_libraries(evolution-calendar
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${LIBSOUP_LDFLAGS}
	${MATH_LDFLAGS}
)

install(TARGETS evolution-calendar
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/calendar/gui
)

install(FILES alarm-notify.ui
	DESTINATION ${uidir}
)

set(DEPENDENCIES
	eabutil
	econtacteditor
	econtactlisteditor
	evolution-calendar
	evolution-calendar-importers
	evolution-mail-composer
	evolution-mail-formatter
	evolution-util
	evolution-shell
)

set(SOURCES
	alarm.c
	alarm.h
	alarm-notify.c
	alarm-notify.h
	alarm-notify-dialog.c
	alarm-notify-dialog.h
	alarm-queue.c
	alarm-queue.h
	config-data.c
	config-data.h
	notify-main.c
	util.c
	util.h
)

add_executable(evolution-alarm-notify
	${SOURCES}
)

add_dependencies(evolution-alarm-notify
	${DEPENDENCIES}
)

target_compile_definitions(evolution-alarm-notify PRIVATE
	-DG_LOG_DOMAIN=\"evolution-alarm-notify\"
	-DEVOLUTION_ICONDIR=\"${icondir}\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
)

target_compile_options(evolution-alarm-notify PUBLIC
	${CANBERRA_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${LIBNOTIFY_CFLAGS}
)

target_include_directories(evolution-alarm-notify PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}
	${CMAKE_BINARY_DIR}/calendar
	${CMAKE_SOURCE_DIR}/calendar
	${CMAKE_CURRENT_BINARY_DIR}
	${CANBERRA_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${LIBNOTIFY_INCLUDE_DIRS}
)

target_link_libraries(evolution-alarm-notify
	${DEPENDENCIES}
	${CANBERRA_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${LIBNOTIFY_LDFLAGS}
)

if(WIN32)
	find_program(WINDRES windres)
	if(WINDRES)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/evolution-alarm-notify-icon.o
			COMMAND ${WINDRES} ${CMAKE_CURRENT_SOURCE_DIR}/evolution-alarm-notify-icon.rc ${CMAKE_CURRENT_BINARY_DIR}/evolution-alarm-notify-icon.o
			DEPENDS evolution-alarm-notify-icon.rc
				evolution-alarm-notify.ico
		)

		target_link_libraries(evolution-alarm-notify
			${CMAKE_CURRENT_BINARY_DIR}/evolution-alarm-notify-icon.o
		)
	endif(WINDRES)
endif(WIN32)

install(TARGETS evolution-alarm-notify
	DESTINATION ${privlibexecdir}
)

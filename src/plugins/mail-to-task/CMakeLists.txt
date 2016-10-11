add_eplug_file(mail-to-task org-gnome-mail-to-task.eplug)

set(DEPENDENCIES
	email-engine
	evolution-calendar
	evolution-mail
	evolution-mail-formatter
	evolution-shell
	evolution-util
)

set(SOURCES
	mail-to-task.c
)

add_library(org-gnome-mail-to-task MODULE
	${SOURCES}
)

add_dependencies(org-gnome-mail-to-task
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-mail-to-task PRIVATE
	-DG_LOG_DOMAIN=\"mail-to-task\"
)

target_compile_options(org-gnome-mail-to-task PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-mail-to-task PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-mail-to-task
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-mail-to-task
	DESTINATION ${plugindir}
)

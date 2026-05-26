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

add_library(module-mail-to-task MODULE
	${SOURCES}
)

set_target_properties(module-mail-to-task PROPERTIES
	PREFIX ""
)

add_dependencies(module-mail-to-task
	${DEPENDENCIES}
)

target_compile_definitions(module-mail-to-task PRIVATE
	-DG_LOG_DOMAIN=\"mail-to-task\"
)

target_compile_options(module-mail-to-task PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-mail-to-task PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-mail-to-task
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-mail-to-task
	DESTINATION ${moduledir}
)

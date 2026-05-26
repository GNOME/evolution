add_error_files(attachment-reminder org-gnome-attachment-reminder.error)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	attachment-reminder.c
)

add_library(module-attachment-reminder MODULE
	${SOURCES}
)

set_target_properties(module-attachment-reminder PROPERTIES
	PREFIX ""
)

add_dependencies(module-attachment-reminder
	${DEPENDENCIES}
)

target_compile_definitions(module-attachment-reminder PRIVATE
	-DG_LOG_DOMAIN=\"attachment-reminder\"
)

target_compile_options(module-attachment-reminder PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-attachment-reminder PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-attachment-reminder
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-attachment-reminder
	DESTINATION ${moduledir}
)

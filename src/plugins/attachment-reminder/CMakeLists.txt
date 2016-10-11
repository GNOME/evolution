add_error_files(attachment-reminder org-gnome-attachment-reminder.error)
add_eplug_file(attachment-reminder org-gnome-evolution-attachment-reminder.eplug)

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

add_library(org-gnome-evolution-attachment-reminder MODULE
	${SOURCES}
)

add_dependencies(org-gnome-evolution-attachment-reminder
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-evolution-attachment-reminder PRIVATE
	-DG_LOG_DOMAIN=\"attachment-reminder\"
)

target_compile_options(org-gnome-evolution-attachment-reminder PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-evolution-attachment-reminder PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-evolution-attachment-reminder
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-evolution-attachment-reminder
	DESTINATION ${plugindir}
)

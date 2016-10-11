add_error_files(external-editor org-gnome-external-editor.error)
add_eplug_file(external-editor org-gnome-external-editor.eplug)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	external-editor.c
)

add_library(org-gnome-external-editor MODULE
	${SOURCES}
)

add_dependencies(org-gnome-external-editor
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-external-editor PRIVATE
	-DG_LOG_DOMAIN=\"external-editor\"
)

target_compile_options(org-gnome-external-editor PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-external-editor PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-external-editor
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-external-editor
	DESTINATION ${plugindir}
)

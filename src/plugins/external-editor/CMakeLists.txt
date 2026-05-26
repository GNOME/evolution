add_error_files(external-editor org-gnome-external-editor.error)

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

add_library(module-external-editor MODULE
	${SOURCES}
)

set_target_properties(module-external-editor PROPERTIES
	PREFIX ""
)

add_dependencies(module-external-editor
	${DEPENDENCIES}
)

target_compile_definitions(module-external-editor PRIVATE
	-DG_LOG_DOMAIN=\"external-editor\"
)

target_compile_options(module-external-editor PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-external-editor PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-external-editor
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-external-editor
	DESTINATION ${moduledir}
)

add_error_files(face org-gnome-face.error)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	face.c
)

add_library(module-face MODULE
	${SOURCES}
)

set_target_properties(module-face PROPERTIES
	PREFIX ""
)

add_dependencies(module-face
	${DEPENDENCIES}
)

target_compile_definitions(module-face PRIVATE
	-DG_LOG_DOMAIN=\"face\"
)

target_compile_options(module-face PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-face PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-face
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-face
	DESTINATION ${moduledir}
)

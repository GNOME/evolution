add_error_files(face org-gnome-face.error)
add_eplug_file(face org-gnome-face.eplug)

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

add_library(org-gnome-face MODULE
	${SOURCES}
)

add_dependencies(org-gnome-face
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-face PRIVATE
	-DG_LOG_DOMAIN=\"face\"
)

target_compile_options(org-gnome-face PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-face PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-face
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-face
	DESTINATION ${plugindir}
)

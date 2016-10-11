add_eplug_file(prefer-plain org-gnome-prefer-plain.eplug)

set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	config-ui.c
)

add_library(org-gnome-prefer-plain MODULE
	${SOURCES}
)

add_dependencies(org-gnome-prefer-plain
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-prefer-plain PRIVATE
	-DG_LOG_DOMAIN=\"prefer-plain\"
)

target_compile_options(org-gnome-prefer-plain PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-prefer-plain PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-prefer-plain
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-prefer-plain
	DESTINATION ${plugindir}
)

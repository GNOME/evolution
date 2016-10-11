add_eplug_file(bbdb org-gnome-evolution-bbdb.eplug)

set(DEPENDENCIES
	eabutil
	econtacteditor
	econtactlisteditor
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	bbdb.c
	bbdb.h
	gaimbuddies.c
)

add_library(org-gnome-evolution-bbdb MODULE
	${SOURCES}
)

add_dependencies(org-gnome-evolution-bbdb
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-evolution-bbdb PRIVATE
	-DG_LOG_DOMAIN=\"bbdb\"
)

target_compile_options(org-gnome-evolution-bbdb PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-evolution-bbdb PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-evolution-bbdb
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-evolution-bbdb
	DESTINATION ${plugindir}
)

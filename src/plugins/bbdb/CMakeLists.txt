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

add_library(module-bbdb MODULE
	${SOURCES}
)

set_target_properties(module-bbdb PROPERTIES
	PREFIX ""
)

add_dependencies(module-bbdb
	${DEPENDENCIES}
)

target_compile_definitions(module-bbdb PRIVATE
	-DG_LOG_DOMAIN=\"bbdb\"
)

target_compile_options(module-bbdb PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-bbdb PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-bbdb
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-bbdb
	DESTINATION ${moduledir}
)

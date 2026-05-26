set(DEPENDENCIES
	email-engine
	evolution-mail
	evolution-shell
	evolution-util
)

set(SOURCES
	dbx-importer.c
)

add_library(module-dbx-import MODULE
	${SOURCES}
)

set_target_properties(module-dbx-import PROPERTIES
	PREFIX ""
)

add_dependencies(module-dbx-import
	${DEPENDENCIES}
)

target_compile_definitions(module-dbx-import PRIVATE
	-DG_LOG_DOMAIN=\"dbx-import\"
)

target_compile_options(module-dbx-import PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-dbx-import PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-dbx-import
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-dbx-import
	DESTINATION ${moduledir}
)

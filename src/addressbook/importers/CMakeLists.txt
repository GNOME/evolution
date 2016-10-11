set(DEPENDENCIES
	eabutil
	evolution-shell
	evolution-util
)

set(SOURCES
	evolution-ldif-importer.c
	evolution-vcard-importer.c
	evolution-csv-importer.c
	evolution-addressbook-importers.h
)

add_library(evolution-addressbook-importers SHARED
	${SOURCES}
)

add_dependencies(evolution-addressbook-importers
	${DEPENDENCIES}
)

target_compile_definitions(evolution-addressbook-importers PRIVATE
	-DG_LOG_DOMAIN=\"evolution-addressbook-importers\"
)

target_compile_options(evolution-addressbook-importers PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-addressbook-importers PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/addressbook
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-addressbook-importers
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-addressbook-importers
	DESTINATION ${privsolibdir}
)

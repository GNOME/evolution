set(DEPENDENCIES
	email-engine
	evolution-mail
	evolution-shell
	evolution-util
)

set(SOURCES
	kmail-libs.c
	kmail-libs.h
	kmail-importer.c
	mail-importer.c
	mail-importer.h
	elm-importer.c
	pine-importer.c
	evolution-mbox-importer.c
)

add_library(evolution-mail-importers SHARED
	${SOURCES}
)

add_dependencies(evolution-mail-importers
	${DEPENDENCIES}
)

target_compile_definitions(evolution-mail-importers PRIVATE
	-DG_LOG_DOMAIN=\"evolution-mail-importers\"
	-DEVOLUTION_PRIVDATADIR=\"${privdatadir}\"
	-DEVOLUTION_ETSPECDIR=\"${etspecdir}\"
)

target_compile_options(evolution-mail-importers PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-mail-importers PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src/mail
	${CMAKE_SOURCE_DIR}/src/mail
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-mail-importers
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-mail-importers
	DESTINATION ${privsolibdir}
)

set(DEPENDENCIES
	evolution-shell
	evolution-util
)

set(SOURCES
	evolution-calendar-importer.h
	icalendar-importer.c
)

add_library(evolution-calendar-importers SHARED
	${SOURCES}
)

add_dependencies(evolution-calendar-importers
	${DEPENDENCIES}
)

target_compile_definitions(evolution-calendar-importers PRIVATE
	-DG_LOG_DOMAIN=\"evolution-calendar-importers\"
	-DEVOLUTION_SOUNDDIR=\"${soundsdir}\"
)

target_compile_options(evolution-calendar-importers PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-calendar-importers PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src/calendar
	${CMAKE_SOURCE_DIR}/src/calendar
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-calendar-importers
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-calendar-importers
	DESTINATION ${privsolibdir}
)

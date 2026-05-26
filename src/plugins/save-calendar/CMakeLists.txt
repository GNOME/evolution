set(DEPENDENCIES
	evolution-calendar
	evolution-shell
	evolution-util
)

set(SOURCES
	save-calendar.c
	ical-format.c
	csv-format.c
	rdf-format.c
	format-handler.h
)

add_library(module-save-calendar MODULE
	${SOURCES}
)

set_target_properties(module-save-calendar PROPERTIES
	PREFIX ""
)

add_dependencies(module-save-calendar
	${DEPENDENCIES}
)

target_compile_definitions(module-save-calendar PRIVATE
	-DG_LOG_DOMAIN=\"save-calendar\"
)

target_compile_options(module-save-calendar PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-save-calendar PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-save-calendar
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-save-calendar
	DESTINATION ${moduledir}
)

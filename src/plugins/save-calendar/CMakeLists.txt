add_eplug_file(save-calendar org-gnome-save-calendar.eplug)

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

add_library(org-gnome-save-calendar MODULE
	${SOURCES}
)

add_dependencies(org-gnome-save-calendar
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-save-calendar PRIVATE
	-DG_LOG_DOMAIN=\"save-calendar\"
)

target_compile_options(org-gnome-save-calendar PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-save-calendar PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-save-calendar
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-save-calendar
	DESTINATION ${plugindir}
)

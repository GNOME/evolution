install(FILES publish-calendar.ui
	DESTINATION ${uidir}
)

set(DEPENDENCIES
	evolution-calendar
	evolution-shell
	evolution-util
)

set(SOURCES
	publish-calendar.c
	publish-format-fb.c
	publish-format-fb.h
	publish-format-ical.c
	publish-format-ical.h
	publish-location.c
	publish-location.h
	url-editor-dialog.c
	url-editor-dialog.h
)

add_library(module-publish-calendar MODULE
	${SOURCES}
)

set_target_properties(module-publish-calendar PROPERTIES
	PREFIX ""
)

add_dependencies(module-publish-calendar
	${DEPENDENCIES}
)

target_compile_definitions(module-publish-calendar PRIVATE
	-DG_LOG_DOMAIN=\"publish-calendar\"
)

target_compile_options(module-publish-calendar PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${LIBNOTIFY_CFLAGS}
)

target_include_directories(module-publish-calendar PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${LIBNOTIFY_INCLUDE_DIRS}
)

target_link_libraries(module-publish-calendar
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${LIBNOTIFY_LDFLAGS}
)

install(TARGETS module-publish-calendar
	DESTINATION ${moduledir}
)

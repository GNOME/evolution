add_error_files(sender-validation org-gnome-sender-validation.error)

set(DEPENDENCIES
	evolution-calendar
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	sender-validation.c
)

add_library(module-sender-validation MODULE
	${SOURCES}
)

set_target_properties(module-sender-validation PROPERTIES
	PREFIX ""
)

add_dependencies(module-sender-validation
	${DEPENDENCIES}
)

target_compile_definitions(module-sender-validation PRIVATE
	-DG_LOG_DOMAIN=\"sender-validation\"
)

target_compile_options(module-sender-validation PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-sender-validation PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-sender-validation
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-sender-validation
	DESTINATION ${moduledir}
)

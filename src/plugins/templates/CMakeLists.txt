set(DEPENDENCIES
	email-engine
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	templates.c
)

add_library(module-templates MODULE
	${SOURCES}
)

set_target_properties(module-templates PROPERTIES
	PREFIX ""
)

add_dependencies(module-templates
	${DEPENDENCIES}
)

target_compile_definitions(module-templates PRIVATE
	-DG_LOG_DOMAIN=\"templates\"
)

target_compile_options(module-templates PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(module-templates PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(module-templates
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS module-templates
	DESTINATION ${moduledir}
)

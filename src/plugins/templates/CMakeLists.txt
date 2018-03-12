add_eplug_file(templates org-gnome-templates.eplug)

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

add_library(org-gnome-templates MODULE
	${SOURCES}
)

add_dependencies(org-gnome-templates
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-templates PRIVATE
	-DG_LOG_DOMAIN=\"templates\"
)

target_compile_options(org-gnome-templates PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-templates PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-templates
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-templates
	DESTINATION ${plugindir}
)

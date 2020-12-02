add_error_files(sender-validation org-gnome-sender-validation.error)
add_eplug_file(sender-validation org-gnome-evolution-sender-validation.eplug)

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

add_library(org-gnome-evolution-sender-validation MODULE
	${SOURCES}
)

add_dependencies(org-gnome-evolution-sender-validation
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-evolution-sender-validation PRIVATE
	-DG_LOG_DOMAIN=\"sender-validation\"
)

target_compile_options(org-gnome-evolution-sender-validation PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-evolution-sender-validation PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-evolution-sender-validation
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-evolution-sender-validation
	DESTINATION ${plugindir}
)

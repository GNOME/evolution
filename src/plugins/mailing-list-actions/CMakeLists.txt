add_error_files(mailing-list-actions org-gnome-mailing-list-actions.error)
add_eplug_file(mailing-list-actions org-gnome-mailing-list-actions.eplug)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	evolution-mail
	evolution-mail-composer
	evolution-mail-formatter
	evolution-shell
	evolution-util
)

set(SOURCES
	mailing-list-actions.c
)

add_library(org-gnome-mailing-list-actions MODULE
	${SOURCES}
)

add_dependencies(org-gnome-mailing-list-actions
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-mailing-list-actions PRIVATE
	-DG_LOG_DOMAIN=\"mailing-list-actions\"
)

target_compile_options(org-gnome-mailing-list-actions PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-mailing-list-actions PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-mailing-list-actions
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-mailing-list-actions
	DESTINATION ${plugindir}
)

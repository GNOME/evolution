add_eplug_file(email-custom-header org-gnome-email-custom-header.eplug)

install(FILES org-gnome-email-custom-header.ui
	DESTINATION ${uidir}
)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	evolution-mail
	evolution-mail-composer
	evolution-shell
	evolution-util
)

set(SOURCES
	email-custom-header.c
	email-custom-header.h
)

add_library(org-gnome-email-custom-header MODULE
	${SOURCES}
)

add_dependencies(org-gnome-email-custom-header
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-email-custom-header PRIVATE
	-DG_LOG_DOMAIN=\"email-custom-header\"
)

target_compile_options(org-gnome-email-custom-header PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-email-custom-header PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-email-custom-header
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-email-custom-header
	DESTINATION ${plugindir}
)

add_eplug_file(mail-notification org-gnome-mail-notification.eplug)

set(DEPENDENCIES
	email-engine
	evolution-mail
	evolution-shell
	evolution-util
)

set(SOURCES
	mail-notification.c
)

add_library(org-gnome-mail-notification MODULE
	${SOURCES}
)

add_dependencies(org-gnome-mail-notification
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-mail-notification PRIVATE
	-DG_LOG_DOMAIN=\"mail-notification\"
)

target_compile_options(org-gnome-mail-notification PUBLIC
	${CANBERRA_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${LIBNOTIFY_CFLAGS}
	${LIBUNITY_CFLAGS}
)

target_include_directories(org-gnome-mail-notification PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CANBERRA_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${LIBNOTIFY_INCLUDE_DIRS}
	${LIBUNITY_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-mail-notification
	${DEPENDENCIES}
	${CANBERRA_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${LIBNOTIFY_LDFLAGS}
	${LIBUNITY_LDFLAGS}
)

install(TARGETS org-gnome-mail-notification
	DESTINATION ${plugindir}
)

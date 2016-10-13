add_error_files(backup-restore org-gnome-backup-restore.error)

set(extra_deps
	email-engine
	evolution-mail
	evolution-shell
)
set(sources
	evolution-backup-restore.c
	e-mail-config-restore-page.c
	e-mail-config-restore-page.h
	e-mail-config-restore-ready-page.c
	e-mail-config-restore-ready-page.h
)
set(extra_defines
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	-DEVOLUTION_TOOLSDIR=\"${privlibexecdir}\"
)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-backup-restore
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)

# ******************************
# evolution-backup binary
# ******************************

set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	evolution-backup-tool.c
)

add_executable(evolution-backup
	${SOURCES}
)

add_dependencies(evolution-backup
	${DEPENDENCIES}
)

target_compile_definitions(evolution-backup PRIVATE
	-DG_LOG_DOMAIN=\"evolution-backup\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	-DDBUS_SERVICES_DIR=\"${SHARE_INSTALL_PREFIX}/dbus-1/services\"
)

target_compile_options(evolution-backup PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-backup PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-backup
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-backup
	DESTINATION ${privlibexecdir}
)

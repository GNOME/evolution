glib_mkenums(e-shell-enumtypes e-shell.h E_SHELL_ENUMTYPES_H)
add_error_files(shell shell.error)

configure_file(evo-version.h.in
	evo-version.h
	@ONLY
)

# ******************************
# evolution-shell library
# ******************************

set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	e-shell.c
	e-shell-backend.c
	e-shell-content.c
	e-shell-enumtypes.c
	e-shell-searchbar.c
	e-shell-sidebar.c
	e-shell-switcher.c
	e-shell-taskbar.c
	e-shell-utils.c
	e-shell-view.c
	e-shell-window.c
	e-shell-window-private.c
	e-shell-window-private.h
	e-shell-migrate.c
	e-shell-migrate.h
	e-shell-window-actions.c
	${CMAKE_CURRENT_BINARY_DIR}/e-shell-enumtypes.c
	${CMAKE_CURRENT_BINARY_DIR}/evo-version.h
)

set(HEADERS
	e-shell.h
	e-shell-backend.h
	e-shell-common.h
	e-shell-content.h
	e-shell-searchbar.h
	e-shell-sidebar.h
	e-shell-switcher.h
	e-shell-taskbar.h
	e-shell-utils.h
	e-shell-view.h
	e-shell-window.h
	e-shell-window-actions.h
	${CMAKE_CURRENT_BINARY_DIR}/e-shell-enumtypes.h
)

add_library(evolution-shell SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(evolution-shell
	${DEPENDENCIES}
)

target_compile_definitions(evolution-shell PRIVATE
	-DG_LOG_DOMAIN=\"evolution-shell\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	-DEVOLUTION_GALVIEWSDIR=\"${viewsdir}\"
	-DEVOLUTION_HELPDIR=\"${evolutionhelpdir}\"
	-DEVOLUTION_ICONDIR=\"${icondir}\"
	-DEVOLUTION_MODULEDIR=\"${moduledir}\"
	-DEVOLUTION_RULEDIR=\"${privdatadir}\"
	-DEVOLUTION_TOOLSDIR=\"${privlibexecdir}\"
	-DE_DATA_SERVER_ICONDIR=\"${E_DATA_SERVER_ICONDIR}\"
)

target_compile_options(evolution-shell PUBLIC
	${CLUTTER_GTK_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-shell PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CLUTTER_GTK_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-shell
	${DEPENDENCIES}
	${CLUTTER_GTK_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-shell
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/shell
)

# ******************************
# evolution binary
# ******************************

set(DEPENDENCIES
	evolution-util
	evolution-shell
)

set(SOURCES
	main.c
	e-convert-local-mail.c
	e-migrate-base-dirs.c
)

add_executable(evolution
	${SOURCES}
)

add_dependencies(evolution
	${DEPENDENCIES}
)

target_compile_definitions(evolution PRIVATE
	-DG_LOG_DOMAIN=\"evolution\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	-DEVOLUTION_GALVIEWSDIR=\"${viewsdir}\"
	-DEVOLUTION_HELPDIR=\"${evolutionhelpdir}\"
	-DEVOLUTION_ICONDIR=\"${icondir}\"
	-DEVOLUTION_ICONDIR_IN_PREFIX=\"${SHARE_INSTALL_PREFIX}/icons\"
	-DEVOLUTION_MODULEDIR=\"${moduledir}\"
	-DEVOLUTION_PREFIX=\"${CMAKE_INSTALL_PREFIX}\"
	-DEVOLUTION_RULEDIR=\"${privdatadir}\"
	-DEVOLUTION_TOOLSDIR=\"${privlibexecdir}\"
	-DE_DATA_SERVER_ICONDIR=\"${E_DATA_SERVER_ICONDIR}\"
)

target_compile_options(evolution PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${CLUTTER_GTK_CFLAGS}
)

target_include_directories(evolution PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${CLUTTER_GTK_INCLUDE_DIRS}
)

target_link_libraries(evolution
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${CLUTTER_GTK_LDFLAGS}
)

if(WIN32)
	find_program(WINDRES windres)
	if(WINDRES)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/evolution-icon.o
			COMMAND ${WINDRES} ${CMAKE_CURRENT_SOURCE_DIR}/evolution-icon.rc ${CMAKE_CURRENT_BINARY_DIR}/evolution-icon.o
			DEPENDS evolution-icon.rc
				evolution.ico
				evolution-mail.ico
				evolution-memos.ico
				evolution-tasks.ico
		)

		target_link_libraries(evolution
			${CMAKE_CURRENT_BINARY_DIR}/evolution-icon.o
		)
	endif(WINDRES)
endif(WIN32)

install(TARGETS evolution
	DESTINATION ${BIN_INSTALL_DIR}
)

# ******************************
# killev binary
# ******************************

if(NOT WIN32)
	set(SOURCES
		killev.c
	)

	add_executable(killev
		${SOURCES}
	)

	target_compile_definitions(killev PRIVATE
		-DG_LOG_DOMAIN=\"killev\"
		-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	)

	target_compile_options(killev PUBLIC
		${EVOLUTION_DATA_SERVER_CFLAGS}
		${GNOME_PLATFORM_CFLAGS}
	)

	target_include_directories(killev PUBLIC
		${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_CURRENT_BINARY_DIR}
		${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
		${GNOME_PLATFORM_INCLUDE_DIRS}
	)

	target_link_libraries(killev
		${DEPENDENCIES}
		${EVOLUTION_DATA_SERVER_LDFLAGS}
		${GNOME_PLATFORM_LDFLAGS}
	)

	install(TARGETS killev
		DESTINATION ${privlibexecdir}
	)
endif(NOT WIN32)

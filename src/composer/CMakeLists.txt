add_error_files(composer mail-composer.error)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	email-engine
	evolution-mail-formatter
	evolution-shell
	evolution-util
)

set(SOURCES
	e-composer-actions.c
	e-composer-from-header.c
	e-composer-header-table.c
	e-composer-header.c
	e-composer-name-header.c
	e-composer-post-header.c
	e-composer-private.c
	e-composer-spell-header.c
	e-composer-text-header.c
	e-msg-composer.c
)

set(HEADERS
	e-composer-actions.h
	e-composer-common.h
	e-composer-from-header.h
	e-composer-header-table.h
	e-composer-header.h
	e-composer-name-header.h
	e-composer-post-header.h
	e-composer-private.h
	e-composer-spell-header.h
	e-composer-text-header.h
	e-msg-composer.h
)

add_library(evolution-mail-composer SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(evolution-mail-composer
	${DEPENDENCIES}
)

target_compile_definitions(evolution-mail-composer PRIVATE
	-DG_LOG_DOMAIN=\"evolution-mail-composer\"
	-DEVOLUTION_UIDIR=\"${uidir}\"
)

target_compile_options(evolution-mail-composer PUBLIC
	${CERT_UI_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-mail-composer PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${ENCHANT_INCLUDE_DIRS}
	${CERT_UI_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-mail-composer
	${DEPENDENCIES}
	${CERT_UI_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-mail-composer
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/composer
)

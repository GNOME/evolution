glib_mkenums(e-mail-engine-enumtypes e-mail-engine-enums.h E_MAIL_ENGINE_ENUMTYPES_H)
add_pkgconfig_file(libemail-engine.pc.in libemail-engine.pc)

set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	camel-null-store.c
	e-mail-folder-utils.c
	e-mail-junk-filter.c
	e-mail-session-utils.c
	e-mail-session.c
	e-mail-store-utils.c
	e-mail-utils.c
	em-filter-folder-element.c
	em-vfolder-context.c
	em-vfolder-rule.c
	mail-config.c
	mail-folder-cache.c
	mail-mt.c
	mail-ops.c
	mail-tools.c
	mail-vfolder.c
	${CMAKE_CURRENT_BINARY_DIR}/e-mail-engine-enumtypes.c
)

set(HEADERS
	libemail-engine.h
	camel-null-store.h
	e-mail-engine-enums.h
	e-mail-folder-utils.h
	e-mail-junk-filter.h
	e-mail-session-utils.h
	e-mail-session.h
	e-mail-store-utils.h
	e-mail-utils.h
	em-filter-folder-element.h
	em-vfolder-context.h
	em-vfolder-rule.h
	mail-config.h
	mail-folder-cache.h
	mail-mt.h
	mail-ops.h
	mail-tools.h
	mail-vfolder.h
	${CMAKE_CURRENT_BINARY_DIR}/e-mail-engine-enumtypes.h
)

add_library(email-engine SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(email-engine
	${DEPENDENCIES}
)

target_compile_definitions(email-engine PRIVATE
	-DG_LOG_DOMAIN=\"e-mail-engine\"
	-DEVOLUTION_PRIVDATADIR=\"${privdatadir}\"
	-DLIBEMAIL_ENGINE_COMPILATION
)

target_compile_options(email-engine PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(email-engine PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(email-engine
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS email-engine
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/libemail-engine
)

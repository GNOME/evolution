set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	e-cert.c
	e-cert.h
	e-cert-trust.c
	e-cert-trust.h
	e-cert-db.c
	e-cert-db.h
	e-pkcs12.c
	e-pkcs12.h
)

add_library(essmime SHARED
	${SOURCES}
)

add_dependencies(essmime
	${DEPENDENCIES}
)

target_compile_definitions(essmime PRIVATE
	-DG_LOG_DOMAIN=\"essmime\"
)

target_compile_options(essmime PUBLIC
	${CERT_UI_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(essmime PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CERT_UI_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(essmime
	${DEPENDENCIES}
	${CERT_UI_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS essmime
	DESTINATION ${privsolibdir}
)

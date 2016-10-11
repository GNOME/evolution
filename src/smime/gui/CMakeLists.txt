set(DEPENDENCIES
	evolution-shell
	evolution-util
	essmime
)

set(SOURCES
	ca-trust-dialog.c
	ca-trust-dialog.h
	cert-trust-dialog.c
	cert-trust-dialog.h
	certificate-manager.c
	certificate-manager.h
	e-cert-selector.c
	e-cert-selector.h
	component.c
	component.h
)

add_library(evolution-smime SHARED
	${SOURCES}
)

add_dependencies(evolution-smime
	${DEPENDENCIES}
)

target_compile_definitions(evolution-smime PRIVATE
	-DG_LOG_DOMAIN=\"evolution-smime\"
)

target_compile_options(evolution-smime PUBLIC
	${CERT_UI_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evolution-smime PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/smime/lib
	${CERT_UI_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evolution-smime
	${DEPENDENCIES}
	${CERT_UI_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evolution-smime
	DESTINATION ${privsolibdir}
)

install(FILES smime-ui.ui
	DESTINATION ${uidir}
)

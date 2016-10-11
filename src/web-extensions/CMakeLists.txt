set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	e-dom-utils.h
	e-dom-utils.c
)

add_library(edomutils SHARED
	${SOURCES}
)

add_dependencies(edomutils
	${DEPENDENCIES}
)

target_compile_definitions(edomutils PRIVATE
	-DG_LOG_DOMAIN=\"edomutils\"
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"
)

target_compile_options(edomutils PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(edomutils PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(edomutils
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS edomutils
	DESTINATION ${privsolibdir}
)

set(SOURCES
	e-web-extension.h
	e-web-extension-names.h
	e-dom-utils.h
	e-web-extension.c
	e-web-extension-main.c
	e-dom-utils.c
)

add_library(ewebextension MODULE
	${SOURCES}
)

add_dependencies(ewebextension
	${DEPENDENCIES}
)

target_compile_definitions(ewebextension PRIVATE
	-DG_LOG_DOMAIN=\"ewebextension\"
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"
)

target_compile_options(ewebextension PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${WEB_EXTENSIONS_CFLAGS}
)

target_include_directories(ewebextension PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${WEB_EXTENSIONS_INCLUDE_DIRS}
)

target_link_libraries(ewebextension
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${WEB_EXTENSIONS_LDFLAGS}
)

install(TARGETS ewebextension
	DESTINATION ${webextensionsdir}
)

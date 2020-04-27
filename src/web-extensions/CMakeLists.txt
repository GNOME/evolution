set(DEPENDENCIES
	evolution-util
)

set(SOURCES
	e-web-extension.h
	e-web-extension.c
	e-web-extension-main.c
)

add_library(ewebextension MODULE
	${SOURCES}
)

add_dependencies(ewebextension
	${DEPENDENCIES}
)

target_compile_definitions(ewebextension PRIVATE
	-DG_LOG_DOMAIN=\"ewebextension\"
	-DEVOLUTION_WEBKITDATADIR=\"${webkitdatadir}\"
	-DEVOLUTION_SOURCE_WEBKITDATADIR=\"${CMAKE_SOURCE_DIR}/data/webkit\"
)

target_compile_options(ewebextension PUBLIC
	${WEB_EXTENSIONS_CFLAGS}
)

target_include_directories(ewebextension PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${WEB_EXTENSIONS_INCLUDE_DIRS}
)

target_link_libraries(ewebextension
	${DEPENDENCIES}
	${WEB_EXTENSIONS_LDFLAGS}
)

install(TARGETS ewebextension
	DESTINATION ${webextensionsdir}
)

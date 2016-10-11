set(DEPENDENCIES
	evolution-shell
	evolution-util
)

set(SOURCES
	eab-book-util.c
	eab-book-util.h
)

add_library(eabutil SHARED
	${SOURCES}
)

add_dependencies(eabutil
	${DEPENDENCIES}
)

target_compile_definitions(eabutil PRIVATE
	-DG_LOG_DOMAIN=\"eabutil\"
)

target_compile_options(eabutil PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(eabutil PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/shell
	${CMAKE_BINARY_DIR}/src/shell
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(eabutil
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS eabutil
	DESTINATION ${privsolibdir}
)

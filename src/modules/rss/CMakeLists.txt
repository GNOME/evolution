pkg_check_modules(LIBEDATASERVER libedataserver-1.2 REQUIRED)
pkg_check_modules(CAMEL camel-1.2 REQUIRED)

set(SOURCES
	camel-rss-store-summary.c
	camel-rss-store-summary.h
	e-rss-parser.h
	e-rss-parser.c
)

add_library(evolution-rss-common SHARED
	${SOURCES}
)

target_compile_definitions(evolution-rss-common PRIVATE
	-DG_LOG_DOMAIN=\"evolution-rss-common\"
)

target_compile_options(evolution-rss-common PUBLIC
	${CAMEL_CFLAGS}
	${LIBEDATASERVER_CFLAGS}
)

target_include_directories(evolution-rss-common PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CAMEL_INCLUDE_DIRS}
	${LIBEDATASERVER_INCLUDE_DIRS}
)

target_link_libraries(evolution-rss-common
	${CAMEL_LDFLAGS}
	${LIBEDATASERVER_LDFLAGS}
)

install(TARGETS evolution-rss-common
	DESTINATION ${privsolibdir}
)

add_subdirectory(camel)
add_subdirectory(evolution)

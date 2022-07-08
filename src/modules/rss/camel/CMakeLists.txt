pkg_check_modules(LIBEDATASERVER libedataserver-1.2 REQUIRED)
pkg_check_modules(CAMEL camel-1.2 REQUIRED)
pkg_check_variable(camel_providerdir camel-1.2 camel_providerdir)

set(sources
	camel-rss-folder.c
	camel-rss-folder.h
	camel-rss-folder-summary.c
	camel-rss-folder-summary.h
	camel-rss-provider.c
	camel-rss-settings.c
	camel-rss-settings.h
	camel-rss-store.c
	camel-rss-store.h
	../camel-rss-store-summary.c
	../camel-rss-store-summary.h
	../e-rss-parser.h
	../e-rss-parser.c
)

add_library(camelrss MODULE ${sources})

target_compile_definitions(camelrss PRIVATE
	-DG_LOG_DOMAIN=\"camel-rss-provider\"
)

target_compile_options(camelrss PUBLIC
	${CAMEL_CFLAGS}
	${LIBEDATASERVER_CFLAGS}
)

target_include_directories(camelrss PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CAMEL_INCLUDE_DIRS}
	${LIBEDATASERVER_INCLUDE_DIRS}
)

target_link_libraries(camelrss
	${CAMEL_LDFLAGS}
	${LIBEDATASERVER_LDFLAGS}
)

install(TARGETS camelrss
	DESTINATION ${camel_providerdir}
)

install(FILES libcamelrss.urls
	DESTINATION ${camel_providerdir}
)

add_eplug_file(dbx-import org-gnome-dbx-import.eplug)

set(DEPENDENCIES
	email-engine
	evolution-mail
	evolution-shell
	evolution-util
)

set(SOURCES
	dbx-importer.c
)

add_library(org-gnome-dbx-import MODULE
	${SOURCES}
)

add_dependencies(org-gnome-dbx-import
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-dbx-import PRIVATE
	-DG_LOG_DOMAIN=\"dbx-import\"
)

target_compile_options(org-gnome-dbx-import PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(org-gnome-dbx-import PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-dbx-import
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS org-gnome-dbx-import
	DESTINATION ${plugindir}
)

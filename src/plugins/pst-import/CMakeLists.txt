add_appdata_file(org.gnome.Evolution-pst.metainfo.xml.in org.gnome.Evolution-pst.metainfo.xml)

set(DEPENDENCIES
	email-engine
	evolution-mail
	evolution-shell
	evolution-util
)

set(SOURCES
	pst-importer.c
)

add_library(module-pst-import MODULE
	${SOURCES}
)

set_target_properties(module-pst-import PROPERTIES
	PREFIX ""
)

add_dependencies(module-pst-import
	${DEPENDENCIES}
)

target_compile_definitions(module-pst-import PRIVATE
	-DG_LOG_DOMAIN=\"pst-import\"
)

target_compile_options(module-pst-import PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${LIBPST_CFLAGS}
)

target_include_directories(module-pst-import PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${LIBPST_INCLUDE_DIRS}
)

target_link_libraries(module-pst-import
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${LIBPST_LDFLAGS}
)

install(TARGETS module-pst-import
	DESTINATION ${moduledir}
)

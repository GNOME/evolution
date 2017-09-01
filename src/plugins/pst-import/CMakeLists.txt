add_eplug_file(pst-import org-gnome-pst-import.eplug)
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

add_library(org-gnome-pst-import MODULE
	${SOURCES}
)

add_dependencies(org-gnome-pst-import
	${DEPENDENCIES}
)

target_compile_definitions(org-gnome-pst-import PRIVATE
	-DG_LOG_DOMAIN=\"pst-import\"
)

target_compile_options(org-gnome-pst-import PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${LIBPST_CFLAGS}
)

target_include_directories(org-gnome-pst-import PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${LIBPST_INCLUDE_DIRS}
)

target_link_libraries(org-gnome-pst-import
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${LIBPST_LDFLAGS}
)

install(TARGETS org-gnome-pst-import
	DESTINATION ${plugindir}
)

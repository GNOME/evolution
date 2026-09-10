install(FILES	addresstypes.xml
		address_formats.dat
		countrytransl.map
	DESTINATION ${privdatadir}
)

set(DEPENDENCIES
	eabutil
	econtactprint
	evolution-shell
	evolution-util
)

set(SOURCES
	eab-config.c
	eab-contact-compare.c
	eab-contact-compare.h
	eab-contact-display.c
	eab-contact-display.h
	eab-contact-formatter.c
	eab-contact-formatter.h
	eab-contact-merging.c
	eab-contact-merging.h
	eab-gui-util.c
	eab-gui-util.h
	e-alphabet-box.c
	e-alphabet-box.h
	e-bulk-edit-contacts.c
	e-bulk-edit-contacts.h
	e-card-view.c
	e-card-view.h
	e-contact-card-box.c
	e-contact-card-box.h
	e-contact-card.c
	e-contact-card.h
	e-addressbook-table.c
	e-addressbook-table.h
	e-addressbook-selector.c
	e-addressbook-selector.h
	e-addressbook-view.c
	e-addressbook-view.h
	gal-view-minicard.c
	gal-view-minicard.h
	ea-addressbook-view.c
	ea-addressbook-view.h
	ea-addressbook.c
	ea-addressbook.h
)

set(HEADERS
	eab-config.h
)

add_library(eabwidgets SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(eabwidgets
	${DEPENDENCIES}
)

target_compile_definitions(eabwidgets PRIVATE
	-DG_LOG_DOMAIN=\"eabwidgets\"
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"
)

target_compile_options(eabwidgets PUBLIC
	${CHAMPLAIN_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GEO_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(eabwidgets PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/addressbook
	${CMAKE_SOURCE_DIR}/src/addressbook/util
	${CHAMPLAIN_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GEO_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(eabwidgets
	${DEPENDENCIES}
	${CHAMPLAIN_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GEO_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS eabwidgets
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/addressbook/gui/widgets
)

# ******************************
# test-addressbook-table-units
# ******************************

add_executable(test-addressbook-table-units
	test-addressbook-table-units.c
)

add_dependencies(test-addressbook-table-units
	eabwidgets
	evolution-util
)

target_compile_definitions(test-addressbook-table-units PRIVATE
	-DG_LOG_DOMAIN=\"test-addressbook-table-units\"
)

target_compile_options(test-addressbook-table-units PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(test-addressbook-table-units PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(test-addressbook-table-units
	eabwidgets
	evolution-util
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

add_check_test(test-addressbook-table-units)
set_tests_properties(test-addressbook-table-units PROPERTIES ENVIRONMENT "GSETTINGS_SCHEMA_DIR=${CMAKE_BINARY_DIR}/data:${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas")
install_test_if_enabled(test-addressbook-table-units "session-exclusive" "")

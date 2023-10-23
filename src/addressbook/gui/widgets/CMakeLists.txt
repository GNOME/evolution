install(FILES	addresstypes.xml
		address_formats.dat
		countrytransl.map
	DESTINATION ${privdatadir}
)

install(FILES e-addressbook-view.etspec
	DESTINATION ${etspecdir}
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
	e-contact-map.c
	e-contact-map.h
	e-contact-map-window.c
	e-contact-map-window.h
	e-addressbook-table-adapter.c
	e-addressbook-table-adapter.h
	e-addressbook-model.c
	e-addressbook-model.h
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
	-DEVOLUTION_ETSPECDIR=\"${etspecdir}\"
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"
	-DEVOLUTION_PRIVDATADIR=\"${privdatadir}\"
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

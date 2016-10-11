set(ecpsdir ${privdatadir}/ecps)

install(FILES	smallbook.ecps
		medbook.ecps
		phonelist.ecps
	DESTINATION ${ecpsdir}
)

set(DEPENDENCIES
	eabutil
	evolution-util
)

set(SOURCES
	e-contact-print-types.h
	e-contact-print.c
	e-contact-print.h
)

add_library(econtactprint SHARED
	${SOURCES}
)

add_dependencies(econtactprint
	${DEPENDENCIES}
)

target_compile_definitions(econtactprint PRIVATE
	-DG_LOG_DOMAIN=\"addressbook-printing\"
	-DEVOLUTION_ECPSDIR=\"${ecpsdir}\"
)

target_compile_options(econtactprint PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(econtactprint PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/addressbook
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(econtactprint
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS econtactprint
	DESTINATION ${privsolibdir}
)

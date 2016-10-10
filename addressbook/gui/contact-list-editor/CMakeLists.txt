install(FILES contact-list-editor.ui
	DESTINATION ${uidir}
)

set(DEPENDENCIES
	eabutil
	eabwidgets
	econtacteditor
	evolution-shell
	evolution-util
)

set(SOURCES
	e-contact-list-editor.c
	e-contact-list-editor.h
	e-contact-list-model.c
	e-contact-list-model.h
)

add_library(econtactlisteditor SHARED
	${SOURCES}
)

add_dependencies(econtactlisteditor
	${DEPENDENCIES}
)

target_compile_definitions(econtactlisteditor PRIVATE
	-DG_LOG_DOMAIN=\"e-contact-list-editor\"
)

target_compile_options(econtactlisteditor PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(econtactlisteditor PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/addressbook
	${CMAKE_SOURCE_DIR}/addressbook/gui/widgets
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(econtactlisteditor
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS econtactlisteditor
	DESTINATION ${privsolibdir}
)

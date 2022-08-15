install(FILES	contact-editor.ui
		fullname.ui
	DESTINATION ${uidir}
)

set(DEPENDENCIES
	eabwidgets
	evolution-shell
	evolution-util
)

if(ENABLE_SMIME)
	list(APPEND DEPENDENCIES essmime)
endif(ENABLE_SMIME)

set(SOURCES
	eab-editor.c
	eab-editor.h
	e-contact-editor.c
	e-contact-editor.h
	e-contact-editor-dyntable.c
	e-contact-editor-dyntable.h
	e-contact-editor-fullname.c
	e-contact-editor-fullname.h
	e-contact-quick-add.c
	e-contact-quick-add.h
)

add_library(econtacteditor SHARED
	${SOURCES}
)

add_dependencies(econtacteditor
	${DEPENDENCIES}
)

target_compile_definitions(econtacteditor PRIVATE
	-DG_LOG_DOMAIN=\"e-contact-editor\"
)

target_compile_options(econtacteditor PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(econtacteditor PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/addressbook
	${CMAKE_SOURCE_DIR}/src/addressbook/gui/widgets
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(econtacteditor
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS econtacteditor
	DESTINATION ${privsolibdir}
)

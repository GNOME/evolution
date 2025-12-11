set(DEPENDENCIES
	eabutil
	eabwidgets
	evolution-shell
	evolution-util
)

if(ENABLE_SMIME)
	list(APPEND DEPENDENCIES essmime)
endif(ENABLE_SMIME)

set(SOURCES
	e-section-box.c
	e-section-box.h
	e-vcard-editor.c
	e-vcard-editor.h
	e-vcard-editor-address.c
	e-vcard-editor-address.h
	e-vcard-editor-certificate.c
	e-vcard-editor-certificate.h
	e-vcard-editor-item.c
	e-vcard-editor-item.h
	e-vcard-editor-notes.c
	e-vcard-editor-notes.h
	e-vcard-editor-section.c
	e-vcard-editor-section.h
)

add_library(evcardeditor SHARED
	${SOURCES}
)

add_dependencies(evcardeditor
	${DEPENDENCIES}
)

target_compile_definitions(evcardeditor PRIVATE
	-DG_LOG_DOMAIN=\"e-vcard-editor\"
)

target_compile_options(evcardeditor PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(evcardeditor PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src/addressbook
	${CMAKE_SOURCE_DIR}/src/addressbook/gui/widgets
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(evcardeditor
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS evcardeditor
	DESTINATION ${privsolibdir}
)

add_executable(test-vcard-editor
	test-vcard-editor.c
)

add_dependencies(test-vcard-editor
	${DEPENDENCIES}
	evcardeditor
)

target_compile_definitions(test-vcard-editor PRIVATE
	-DG_LOG_DOMAIN=\"test-vcard-editor\"
	-DEVOLUTION_ICONDIR=\"${icondir}\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	-DEVOLUTION_MODULEDIR=\"${moduledir}\"
	-DEVOLUTION_SOURCE_WEBKITDATADIR=\"${CMAKE_SOURCE_DIR}/data/webkit\"
	-DEVOLUTION_TESTGIOMODULESDIR=\"${CMAKE_CURRENT_BINARY_DIR}/test-gio-modules\"
	-DEVOLUTION_TESTTOPSRCDIR=\"${CMAKE_SOURCE_DIR}\"
	-DE_DATA_SERVER_ICONDIR=\"${E_DATA_SERVER_ICONDIR}\"
)

target_compile_options(test-vcard-editor PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(test-vcard-editor PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(test-vcard-editor
	${DEPENDENCIES}
	evcardeditor
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${MATH_LDFLAGS}
)

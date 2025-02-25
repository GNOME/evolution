glib_mkenums(e-mail-enumtypes e-mail-enums.h E_MAIL_ENUMTYPES_H)
add_error_files(mail mail.error)
add_custom_xml_files(mail ${privdatadir} filter .in ""
	filtertypes.xml
	searchtypes.xml
	vfoldertypes.xml
)

install(FILES message-list.etspec
	DESTINATION ${etspecdir}
)

install(FILES	mail-config.ui
		mail-dialogs.ui
	DESTINATION ${uidir}
)

set(DEPENDENCIES
	econtacteditor
	econtactlisteditor
	email-engine
	evolution-mail-composer
	evolution-mail-formatter
	evolution-shell
	evolution-util
)

if(ENABLE_SMIME)
	list(APPEND DEPENDENCIES
		essmime
		evolution-smime
	)
endif(ENABLE_SMIME)

set(SOURCES
	e-cid-request.c
	e-http-request.c
	e-mail-account-manager.c
	e-mail-account-store.c
	e-mail-account-tree-view.c
	e-mail-autoconfig.c
	e-mail-backend.c
	e-mail-browser.c
	e-mail-config-activity-page.c
	e-mail-config-assistant.c
	e-mail-config-auth-check.c
	e-mail-config-composing-page.c
	e-mail-config-confirm-page.c
	e-mail-config-defaults-page.c
	e-mail-config-identity-page.c
	e-mail-config-lookup-page.c
	e-mail-config-notebook.c
	e-mail-config-page.c
	e-mail-config-provider-page.c
	e-mail-config-receiving-page.c
	e-mail-config-security-page.c
	e-mail-config-sending-page.c
	e-mail-config-service-backend.c
	e-mail-config-service-notebook.c
	e-mail-config-service-page.c
	e-mail-config-sidebar.c
	e-mail-config-summary-page.c
	e-mail-config-welcome-page.c
	e-mail-config-window.c
	e-mail-display.c
	e-mail-display-popup-extension.c
	e-mail-folder-create-dialog.c
	e-mail-folder-sort-order-dialog.c
	e-mail-folder-tweaks.c
	e-mail-free-form-exp.c
	e-mail-junk-options.c
	e-mail-label-dialog.c
	e-mail-label-list-store.c
	e-mail-label-manager.c
	e-mail-label-tree-view.c
	e-mail-message-pane.c
	e-mail-migrate.c
	e-mail-notes.c
	e-mail-paned-view.c
	e-mail-print-config-headers.c
	e-mail-printer.c
	e-mail-properties.c
	e-mail-reader-utils.c
	e-mail-reader.c
	e-mail-remote-content.c
	e-mail-remote-content-popover.c
	e-mail-request.c
	e-mail-send-account-override.c
	e-mail-sidebar.c
	e-mail-tag-editor.c
	e-mail-templates.c
	e-mail-templates-store.c
	e-mail-ui-session.c
	e-mail-view.c
	e-mail-viewer.c
	em-composer-utils.c
	em-config.c
	em-event.c
	em-filter-context.c
	em-filter-editor.c
	em-filter-editor-folder-element.c
	em-filter-mail-identity-element.c
	em-filter-rule.c
	em-filter-source-element.c
	em-folder-properties.c
	em-folder-selection-button.c
	em-folder-selector.c
	em-folder-tree-model.c
	em-folder-tree.c
	em-folder-utils.c
	em-search-context.c
	em-subscription-editor.c
	em-utils.c
	em-vfolder-editor-context.c
	em-vfolder-editor-rule.c
	em-vfolder-editor.c
	mail-autofilter.c
	mail-send-recv.c
	mail-vfolder-ui.c
	message-list.c
	${CMAKE_CURRENT_BINARY_DIR}/e-mail-enumtypes.c
)

set(HEADERS
	e-cid-request.h
	e-http-request.h
	e-mail.h
	e-mail-account-manager.h
	e-mail-account-store.h
	e-mail-account-tree-view.h
	e-mail-autoconfig.h
	e-mail-backend.h
	e-mail-browser.h
	e-mail-config-activity-page.h
	e-mail-config-assistant.h
	e-mail-config-auth-check.h
	e-mail-config-composing-page.h
	e-mail-config-confirm-page.h
	e-mail-config-defaults-page.h
	e-mail-config-identity-page.h
	e-mail-config-lookup-page.h
	e-mail-config-notebook.h
	e-mail-config-page.h
	e-mail-config-provider-page.h
	e-mail-config-receiving-page.h
	e-mail-config-security-page.h
	e-mail-config-sending-page.h
	e-mail-config-service-backend.h
	e-mail-config-service-notebook.h
	e-mail-config-service-page.h
	e-mail-config-sidebar.h
	e-mail-config-summary-page.h
	e-mail-config-welcome-page.h
	e-mail-config-window.h
	e-mail-display.h
	e-mail-display-popup-extension.h
	e-mail-enums.h
	e-mail-folder-create-dialog.h
	e-mail-folder-sort-order-dialog.h
	e-mail-folder-tweaks.h
	e-mail-free-form-exp.h
	e-mail-junk-options.h
	e-mail-label-dialog.h
	e-mail-label-list-store.h
	e-mail-label-manager.h
	e-mail-label-tree-view.h
	e-mail-message-pane.h
	e-mail-migrate.h
	e-mail-notes.h
	e-mail-paned-view.h
	e-mail-print-config-headers.h
	e-mail-printer.h
	e-mail-properties.h
	e-mail-reader-utils.h
	e-mail-reader.h
	e-mail-remote-content.h
	e-mail-remote-content-popover.h
	e-mail-request.h
	e-mail-send-account-override.h
	e-mail-sidebar.h
	e-mail-tag-editor.h
	e-mail-templates.h
	e-mail-templates-store.h
	e-mail-ui-session.h
	e-mail-view.h
	e-mail-viewer.h
	em-composer-utils.h
	em-config.h
	em-event.h
	em-filter-context.h
	em-filter-editor.h
	em-filter-editor-folder-element.h
	em-filter-mail-identity-element.h
	em-filter-rule.h
	em-filter-source-element.h
	em-folder-properties.h
	em-folder-selection-button.h
	em-folder-selector.h
	em-folder-tree-model.h
	em-folder-tree.h
	em-folder-utils.h
	em-search-context.h
	em-subscription-editor.h
	em-utils.h
	em-vfolder-editor-context.h
	em-vfolder-editor-rule.h
	em-vfolder-editor.h
	mail-autofilter.h
	mail-send-recv.h
	mail-vfolder-ui.h
	message-list.h
	${CMAKE_CURRENT_BINARY_DIR}/e-mail-enumtypes.h
)

add_library(evolution-mail SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(evolution-mail
	${DEPENDENCIES}
)

target_compile_definitions(evolution-mail PRIVATE
	-DG_LOG_DOMAIN=\"evolution-mail\"
	-DEVOLUTION_PRIVDATADIR=\"${privdatadir}\"
	-DEVOLUTION_ETSPECDIR=\"${etspecdir}\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
)

target_compile_options(evolution-mail PUBLIC
	${CERT_UI_CFLAGS}
	${CANBERRA_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${SQLITE3_CFLAGS}
)

target_include_directories(evolution-mail PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src/composer
	${CMAKE_BINARY_DIR}/src/em-format
	${CMAKE_BINARY_DIR}/src/shell
	${CMAKE_BINARY_DIR}/src/smime/lib
	${CMAKE_BINARY_DIR}/src/smime/gui
	${CMAKE_SOURCE_DIR}/src/composer
	${CMAKE_SOURCE_DIR}/src/em-format
	${CMAKE_SOURCE_DIR}/src/shell
	${CMAKE_SOURCE_DIR}/src/smime/lib
	${CMAKE_SOURCE_DIR}/src/smime/gui
	${CMAKE_CURRENT_SOURCE_DIR}
	${CERT_UI_INCLUDE_DIRS}
	${CANBERRA_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(evolution-mail
	${DEPENDENCIES}
	${CERT_UI_LDFLAGS}
	${CANBERRA_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${SQLITE3_LDFLAGS}
)

install(TARGETS evolution-mail
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/mail
)

# ******************************
# test-mail-autoconfig
# ******************************

add_executable(test-mail-autoconfig
	e-mail-autoconfig.c
	e-mail-autoconfig.h
	test-mail-autoconfig.c
)

add_dependencies(test-mail-autoconfig
	evolution-util
)

target_compile_definitions(test-mail-autoconfig PRIVATE
	-DG_LOG_DOMAIN=\"test-mail-autoconfig\"
)

target_compile_options(test-mail-autoconfig PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(test-mail-autoconfig PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(test-mail-autoconfig
	evolution-util
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

add_subdirectory(default)
add_subdirectory(importers)

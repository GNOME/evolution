glib_mkenums(e-util-enumtypes e-util-enums.h E_UTIL_ENUMTYPES_H)
glib_genmarshal(e-marshal e_marshal e-marshal.list)

add_error_files(e-util
	e-system.error
	filter.error
	widgets.error
)

set(uifiles
	e-table-config.ui
	e-timezone-dialog.ui
	filter.ui
	gal-view-instance-save-as-dialog.ui
)

install(FILES ${uifiles}
	DESTINATION ${uidir}
)

set(PLATFORM_SOURCES)

if(WIN32)
	set(PLATFORM_SOURCES
		e-win32-reloc.c
		e-win32-defaults.c
		e-win32-defaults.h
	)
endif(WIN32)

glib_compile_resources(${CMAKE_SOURCE_DIR}/data/ ${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.gresource org_gnome_evolution ../../data/org.gnome.Evolution.gresource.xml)

# Main libevolution-util

set(DEPENDENCIES
	gnomecanvas
)

set(SOURCES
	e-accounts-window.c
	e-action-combo-box.c
	e-activity-bar.c
	e-activity-proxy.c
	e-activity.c
	e-alarm-selector.c
	e-alert-bar.c
	e-alert-dialog.c
	e-alert-sink.c
	e-alert.c
	e-attachment-bar.c
	e-attachment-handler-image.c
	e-attachment-handler.c
	e-attachment-icon-view.c
	e-attachment-paned.c
	e-attachment-popover.c
	e-attachment-store.c
	e-attachment-tree-view.c
	e-attachment-view.c
	e-attachment.c
	e-auth-combo-box.c
	e-autocomplete-selector.c
	e-bit-array.c
	e-book-source-config.c
	e-cal-source-config.c
	e-calendar-item.c
	e-calendar.c
	e-canvas-background.c
	e-canvas-utils.c
	e-canvas-vbox.c
	e-canvas.c
	e-categories-config.c
	e-categories-dialog.c
	e-categories-editor.c
	e-categories-selector.c
	e-category-completion.c
	e-category-editor.c
	e-cell-checkbox.c
	e-cell-combo.c
	e-cell-date-edit.c
	e-cell-date-int.c
	e-cell-date.c
	e-cell-hbox.c
	e-cell-number.c
	e-cell-percent.c
	e-cell-pixbuf.c
	e-cell-popup.c
	e-cell-size.c
	e-cell-text.c
	e-cell-toggle.c
	e-cell-tree.c
	e-cell-vbox.c
	e-cell.c
	e-charset-combo-box.c
	e-charset.c
	e-client-cache.c
	e-client-combo-box.c
	e-client-selector.c
	e-collection-account-wizard.c
	e-color-chooser-widget.c
	e-color-combo.c
	e-color-scheme-watcher.c
	e-config.c
	e-config-lookup.c
	e-config-lookup-result.c
	e-config-lookup-result-simple.c
	e-config-lookup-worker.c
	e-conflict-search-selector.c
	e-contact-store.c
	e-content-editor.c
	e-content-request.c
	e-data-capture.c
	e-dateedit.c
	e-datetime-format.c
	e-destination-store.c
	e-dialog-utils.c
	e-dialog-widgets.c
	e-ellipsized-combo-box-text.c
	e-emoticon-chooser-menu.c
	e-emoticon-chooser.c
	e-emoticon.c
	e-event.c
	e-file-request.c
	e-file-utils.c
	e-filter-code.c
	e-filter-color.c
	e-filter-datespec.c
	e-filter-element.c
	e-filter-file.c
	e-filter-input.c
	e-filter-int.c
	e-filter-label.c
	e-filter-option.c
	e-filter-part.c
	e-filter-rule.c
	e-focus-tracker.c
	e-gtkemojichooser.h
	e-gtkemojichooser.c
	e-headerbar.c
	e-headerbar-button.c
	e-html-editor-actions.c
	e-html-editor-cell-dialog.c
	e-html-editor-dialog.c
	e-html-editor-find-dialog.c
	e-html-editor-hrule-dialog.c
	e-html-editor-image-dialog.c
	e-html-editor-link-popover.c
	e-html-editor-page-dialog.c
	e-html-editor-paragraph-dialog.c
	e-html-editor-private.h
	e-html-editor-replace-dialog.c
	e-html-editor-spell-check-dialog.c
	e-html-editor-table-dialog.c
	e-html-editor-text-dialog.c
	e-html-editor.c
	e-html-utils.c
	e-icon-factory.c
	e-image-chooser.c
	e-image-chooser-dialog.c
	e-import-assistant.c
	e-import.c
	e-interval-chooser.c
	e-mail-identity-combo-box.c
	e-mail-signature-combo-box.c
	e-mail-signature-editor.c
	e-mail-signature-manager.c
	e-mail-signature-preview.c
	e-mail-signature-script-dialog.c
	e-mail-signature-tree-view.c
	e-map.c
	e-markdown-editor.c
	e-markdown-utils.c
	e-marshal.c
	e-menu-bar.c
	e-menu-tool-button.c
	e-misc-utils.c
	e-mktemp.c
	e-month-widget.c
	e-name-selector-dialog.c
	e-name-selector-entry.c
	e-name-selector-list.c
	e-name-selector-model.c
	e-name-selector.c
	e-online-button.c
	e-paned.c
	e-passwords.c
	e-photo-cache.c
	e-photo-source.c
	e-picture-gallery.c
	e-plugin-ui.c
	e-plugin.c
	e-poolv.c
	e-popup-menu.c
	e-port-entry.c
	e-preferences-window.c
	e-preview-pane.c
	e-print.c
	e-printable.c
	e-proxy-combo-box.c
	e-proxy-editor.c
	e-proxy-link-selector.c
	e-proxy-preferences.c
	e-proxy-selector.c
	e-rule-context.c
	e-rule-editor.c
	e-search-bar.c
	e-selectable.c
	e-selection-model-array.c
	e-selection-model-simple.c
	e-selection-model.c
	e-selection.c
	e-simple-async-result.c
	e-sorter-array.c
	e-sorter.c
	e-source-combo-box.c
	e-source-config-backend.c
	e-source-config-dialog.c
	e-source-config.c
	e-source-conflict-search.c
	e-source-selector-dialog.c
	e-source-selector.c
	e-source-util.c
	e-spell-checker.c
	e-spell-dictionary.c
	e-spell-entry.c
	e-spell-text-view.c
	e-spinner.c
	e-split-date-edit.c
	e-stock-request.c
	e-supported-locales-private.h
	e-table-click-to-add.c
	e-table-col.c
	e-table-column-selector.c
	e-table-column-specification.c
	e-table-config.c
	e-table-extras.c
	e-table-field-chooser-dialog.c
	e-table-field-chooser-item.c
	e-table-field-chooser.c
	e-table-group-container.c
	e-table-group-leaf.c
	e-table-group.c
	e-table-header-item.c
	e-table-header-utils.c
	e-table-header.c
	e-table-item.c
	e-table-model.c
	e-table-one.c
	e-table-search.c
	e-table-selection-model.c
	e-table-sort-info.c
	e-table-sorted-variable.c
	e-table-sorted.c
	e-table-sorter.c
	e-table-sorting-utils.c
	e-table-specification.c
	e-table-state.c
	e-table-subset-variable.c
	e-table-subset.c
	e-table-utils.c
	e-table.c
	e-text-event-processor-emacs-like.c
	e-text-event-processor.c
	e-text-model-repos.c
	e-text-model.c
	e-text.c
	e-timezone-dialog.c
	e-tree-model-generator.c
	e-tree-model.c
	e-tree-selection-model.c
	e-tree-table-adapter.c
	e-tree-view-frame.c
	e-tree.c
	e-ui-action.c
	e-ui-action-group.c
	e-ui-customize-dialog.c
	e-ui-customizer.c
	e-ui-manager.c
	e-ui-menu.c
	e-ui-parser.c
	e-unicode.c
	e-url-entry.c
	e-util-private.h
	e-webdav-browser.c
	e-web-view.c
	e-web-view-jsc-utils.c
	e-web-view-preview.c
	e-widget-undo.c
	e-xml-utils.c
	ea-calendar-cell.c
	ea-calendar-item.c
	ea-cell-table.c
	ea-widgets.c
	gal-a11y-e-cell-popup.c
	gal-a11y-e-cell-registry.c
	gal-a11y-e-cell-text.c
	gal-a11y-e-cell-toggle.c
	gal-a11y-e-cell-tree.c
	gal-a11y-e-cell-vbox.c
	gal-a11y-e-cell.c
	gal-a11y-e-table-click-to-add-factory.c
	gal-a11y-e-table-click-to-add.c
	gal-a11y-e-table-column-header.c
	gal-a11y-e-table-item-factory.c
	gal-a11y-e-table-item.c
	gal-a11y-e-table.c
	gal-a11y-e-text-factory.c
	gal-a11y-e-text.c
	gal-a11y-e-tree.c
	gal-a11y-util.c
	gal-view-collection.c
	gal-view-etable.c
	gal-view-instance-save-as-dialog.c
	gal-view-instance.c
	gal-view.c
	${PLATFORM_SOURCES}
	${CMAKE_CURRENT_BINARY_DIR}/e-marshal.c
	${CMAKE_CURRENT_BINARY_DIR}/e-util-enumtypes.c
	${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.gresource.c
	${CMAKE_CURRENT_BINARY_DIR}/org.gnome.Evolution.gresource.h
)

set(HEADERS
	e-util.h
	e-accounts-window.h
	e-action-combo-box.h
	e-activity-bar.h
	e-activity-proxy.h
	e-activity.h
	e-alarm-selector.h
	e-alert-bar.h
	e-alert-dialog.h
	e-alert-sink.h
	e-alert.h
	e-attachment-bar.h
	e-attachment-handler-image.h
	e-attachment-handler.h
	e-attachment-icon-view.h
	e-attachment-paned.h
	e-attachment-popover.h
	e-attachment-store.h
	e-attachment-tree-view.h
	e-attachment-view.h
	e-attachment.h
	e-auth-combo-box.h
	e-autocomplete-selector.h
	e-bit-array.h
	e-book-source-config.h
	e-cal-source-config.h
	e-calendar-item.h
	e-calendar.h
	e-canvas-background.h
	e-canvas-utils.h
	e-canvas-vbox.h
	e-canvas.h
	e-categories-config.h
	e-categories-dialog.h
	e-categories-editor.h
	e-categories-selector.h
	e-category-completion.h
	e-category-editor.h
	e-cell-checkbox.h
	e-cell-combo.h
	e-cell-date-edit.h
	e-cell-date-int.h
	e-cell-date.h
	e-cell-hbox.h
	e-cell-number.h
	e-cell-percent.h
	e-cell-pixbuf.h
	e-cell-popup.h
	e-cell-size.h
	e-cell-text.h
	e-cell-toggle.h
	e-cell-tree.h
	e-cell-vbox.h
	e-cell.h
	e-charset-combo-box.h
	e-charset.h
	e-client-cache.h
	e-client-combo-box.h
	e-client-selector.h
	e-collection-account-wizard.h
	e-color-chooser-widget.h
	e-color-combo.h
	e-color-scheme-watcher.h
	e-config.h
	e-config-lookup.h
	e-config-lookup-result.h
	e-config-lookup-result-simple.h
	e-config-lookup-worker.h
	e-conflict-search-selector.h
	e-contact-store.h
	e-content-editor.h
	e-content-request.h
	e-data-capture.h
	e-dateedit.h
	e-datetime-format.h
	e-destination-store.h
	e-dialog-utils.h
	e-dialog-widgets.h
	e-ellipsized-combo-box-text.h
	e-emoticon-chooser-menu.h
	e-emoticon-chooser.h
	e-emoticon.h
	e-event.h
	e-file-request.h
	e-file-utils.h
	e-filter-code.h
	e-filter-color.h
	e-filter-datespec.h
	e-filter-element.h
	e-filter-file.h
	e-filter-input.h
	e-filter-int.h
	e-filter-label.h
	e-filter-option.h
	e-filter-part.h
	e-filter-rule.h
	e-focus-tracker.h
	e-headerbar.h
	e-headerbar-button.h
	e-html-editor-actions.h
	e-html-editor-cell-dialog.h
	e-html-editor-dialog.h
	e-html-editor-find-dialog.h
	e-html-editor-hrule-dialog.h
	e-html-editor-image-dialog.h
	e-html-editor-link-popover.h
	e-html-editor-page-dialog.h
	e-html-editor-paragraph-dialog.h
	e-html-editor-replace-dialog.h
	e-html-editor-spell-check-dialog.h
	e-html-editor-table-dialog.h
	e-html-editor-text-dialog.h
	e-html-editor.h
	e-html-utils.h
	e-icon-factory.h
	e-image-chooser.h
	e-image-chooser-dialog.h
	e-import-assistant.h
	e-import.h
	e-interval-chooser.h
	e-mail-identity-combo-box.h
	e-mail-signature-combo-box.h
	e-mail-signature-editor.h
	e-mail-signature-manager.h
	e-mail-signature-preview.h
	e-mail-signature-script-dialog.h
	e-mail-signature-tree-view.h
	e-map.h
	e-markdown-editor.h
	e-markdown-utils.h
	e-menu-bar.h
	e-menu-tool-button.h
	e-misc-utils.h
	e-mktemp.h
	e-month-widget.h
	e-name-selector-dialog.h
	e-name-selector-entry.h
	e-name-selector-list.h
	e-name-selector-model.h
	e-name-selector.h
	e-online-button.h
	e-paned.h
	e-passwords.h
	e-photo-cache.h
	e-photo-source.h
	e-picture-gallery.h
	e-plugin-ui.h
	e-plugin.h
	e-poolv.h
	e-popup-menu.h
	e-port-entry.h
	e-preferences-window.h
	e-preview-pane.h
	e-print.h
	e-printable.h
	e-proxy-combo-box.h
	e-proxy-editor.h
	e-proxy-link-selector.h
	e-proxy-preferences.h
	e-proxy-selector.h
	e-rule-context.h
	e-rule-editor.h
	e-search-bar.h
	e-selectable.h
	e-selection-model-array.h
	e-selection-model-simple.h
	e-selection-model.h
	e-selection.h
	e-simple-async-result.h
	e-sorter-array.h
	e-sorter.h
	e-source-combo-box.h
	e-source-config-backend.h
	e-source-config-dialog.h
	e-source-config.h
	e-source-conflict-search.h
	e-source-selector-dialog.h
	e-source-selector.h
	e-source-util.h
	e-spell-checker.h
	e-spell-dictionary.h
	e-spell-entry.h
	e-spell-text-view.h
	e-spinner.h
	e-split-date-edit.h
	e-stock-request.h
	e-table-click-to-add.h
	e-table-col-dnd.h
	e-table-col.h
	e-table-column-selector.h
	e-table-column-specification.h
	e-table-config.h
	e-table-defines.h
	e-table-extras.h
	e-table-field-chooser-dialog.h
	e-table-field-chooser-item.h
	e-table-field-chooser.h
	e-table-group-container.h
	e-table-group-leaf.h
	e-table-group.h
	e-table-header-item.h
	e-table-header-utils.h
	e-table-header.h
	e-table-item.h
	e-table-model.h
	e-table-one.h
	e-table-search.h
	e-table-selection-model.h
	e-table-sort-info.h
	e-table-sorted-variable.h
	e-table-sorted.h
	e-table-sorter.h
	e-table-sorting-utils.h
	e-table-specification.h
	e-table-state.h
	e-table-subset-variable.h
	e-table-subset.h
	e-table-utils.h
	e-table.h
	e-text-event-processor-emacs-like.h
	e-text-event-processor-types.h
	e-text-event-processor.h
	e-text-model-repos.h
	e-text-model.h
	e-text.h
	e-timezone-dialog.h
	e-tree-model-generator.h
	e-tree-model.h
	e-tree-selection-model.h
	e-tree-table-adapter.h
	e-tree-view-frame.h
	e-tree.h
	e-ui-action.h
	e-ui-action-group.h
	e-ui-customize-dialog.h
	e-ui-customizer.h
	e-ui-manager.h
	e-ui-menu.h
	e-ui-parser.h
	e-unicode.h
	e-url-entry.h
	e-util-enums.h
	e-webdav-browser.h
	e-web-view.h
	e-web-view-jsc-utils.h
	e-web-view-preview.h
	e-widget-undo.h
	e-xml-utils.h
	ea-calendar-cell.h
	ea-calendar-item.h
	ea-cell-table.h
	ea-factory.h
	ea-widgets.h
	gal-a11y-e-cell-popup.h
	gal-a11y-e-cell-registry.h
	gal-a11y-e-cell-text.h
	gal-a11y-e-cell-toggle.h
	gal-a11y-e-cell-tree.h
	gal-a11y-e-cell-vbox.h
	gal-a11y-e-cell.h
	gal-a11y-e-table-click-to-add-factory.h
	gal-a11y-e-table-click-to-add.h
	gal-a11y-e-table-column-header.h
	gal-a11y-e-table-item-factory.h
	gal-a11y-e-table-item.h
	gal-a11y-e-table.h
	gal-a11y-e-text-factory.h
	gal-a11y-e-text.h
	gal-a11y-e-tree.h
	gal-a11y-factory.h
	gal-a11y-util.h
	gal-view-collection.h
	gal-view-etable.h
	gal-view-instance-save-as-dialog.h
	gal-view-instance.h
	gal-view.h
	${CMAKE_CURRENT_BINARY_DIR}/e-marshal.h
	${CMAKE_CURRENT_BINARY_DIR}/e-util-enumtypes.h
)

add_library(evolution-util SHARED
	${SOURCES}
	${HEADERS}
)

add_dependencies(evolution-util
	${DEPENDENCIES}
)

target_compile_definitions(evolution-util PRIVATE
	-DG_LOG_DOMAIN=\"evolution-util\"
	-DEVOLUTION_BINDIR=\"${BIN_INSTALL_DIR}\"
	-DEVOLUTION_DATADIR=\"${SHARE_INSTALL_PREFIX}\"
	-DEVOLUTION_ECPSDIR=\"${privdatadir}/ecps\"
	-DEVOLUTION_ETSPECDIR=\"${etspecdir}\"
	-DEVOLUTION_GALVIEWSDIR=\"${viewsdir}\"
	-DEVOLUTION_HELPDIR=\"${evolutionhelpdir}\"
	-DEVOLUTION_ICONDIR=\"${icondir}\"
	-DEVOLUTION_IMAGESDIR=\"${imagesdir}\"
	-DEVOLUTION_LIBDIR=\"${LIB_INSTALL_DIR}\"
	-DEVOLUTION_LIBEXECDIR=\"${privlibexecdir}\"
	-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
	-DEVOLUTION_MODULEDIR=\"${moduledir}\"
	-DEVOLUTION_PLUGINDIR=\"${plugindir}\"
	-DEVOLUTION_PREFIX=\"${CMAKE_INSTALL_PREFIX}\"
	-DEVOLUTION_PRIVDATADIR=\"${privdatadir}\"
	-DEVOLUTION_SOUNDDIR=\"${soundsdir}\"
	-DEVOLUTION_SYSCONFDIR=\"${SYSCONF_INSTALL_DIR}\"
	-DEVOLUTION_TOOLSDIR=\"${privlibexecdir}\"
	-DEVOLUTION_UIDIR=\"${uidir}\"
	-DEVOLUTION_RULEDIR=\"${privdatadir}\"
	-DEVOLUTION_WEB_EXTENSIONS_DIR=\"${webextensionsdir}\"
	-DEVOLUTION_WEBKITDATADIR=\"${webkitdatadir}\"
	-DEVOLUTION_TESTGIOMODULESDIR=\"${CMAKE_CURRENT_BINARY_DIR}\"
	-DEVOLUTION_TESTTOPSRCDIR=\"${CMAKE_SOURCE_DIR}\"
	-DE_DATA_SERVER_ICONDIR=\"${E_DATA_SERVER_ICONDIR}\"
	-DLIBEUTIL_COMPILATION
)

target_compile_options(evolution-util PUBLIC
	${AUTOAR_CFLAGS}
	${ENCHANT_CFLAGS}
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GEO_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
	${GSPELL_CFLAGS}
	${LIBSECRET_CFLAGS}
)

target_include_directories(evolution-util PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${AUTOAR_INCLUDE_DIRS}
	${ENCHANT_INCLUDE_DIRS}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GEO_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
	${GSPELL_INCLUDE_DIRS}
	${LIBSECRET_INCLUDE_DIRS}
	${MARKDOWN_INCLUDE_DIRS}
)

target_link_libraries(evolution-util
	${DEPENDENCIES}
	${AUTOAR_LDFLAGS}
	${ENCHANT_LDFLAGS}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GEO_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
	${GSPELL_LDFLAGS}
	${ICONV_LIBS}
	${LIBSECRET_LDFLAGS}
	${MATH_LDFLAGS}
	${MARKDOWN_LDFLAGS}
)

if(HAVE_LDAP)
	target_compile_options(evolution-util PUBLIC
		${LDAP_CFLAGS}
	)

	target_include_directories(evolution-util PUBLIC
		${LDAP_INCLUDE_DIRS}
	)

	target_link_libraries(evolution-util
		${LDAP_LIBS}
	)
endif(HAVE_LDAP)

install(TARGETS evolution-util
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/e-util
)

# Private gsettings module

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test-gio-modules
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/test-gio-modules
)

add_custom_target(test-gio-modules-dir
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test-gio-modules
)

# This is not meant to be distributed to end users, unless test-html-editor-units
# is also distributed, but even then it can work without it
add_library(evolutiontestsettings MODULE
	test-keyfile-settings-backend.h
	test-keyfile-settings-backend.c
)

add_dependencies(evolutiontestsettings
	test-gio-modules-dir
)

set_target_properties(evolutiontestsettings PROPERTIES
	PREFIX ""
	OUTPUT_NAME test-gio-modules/libevolutiontestsettings
)

target_compile_definitions(evolutiontestsettings PRIVATE
	-DG_LOG_DOMAIN=\"evolution-test-settings\"
)

target_compile_options(evolutiontestsettings PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
)

target_include_directories(evolutiontestsettings PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
)

target_link_libraries(evolutiontestsettings
	${EVOLUTION_DATA_SERVER_LDFLAGS}
)

# Private test programs

macro(add_private_program _name _sources)
	set(DEPENDENCIES
		evolution-util
	)

	set(SOURCES)
	foreach(_src ${_sources} ${ARGN})
		list(APPEND SOURCES ${_src})
	endforeach(_src)

	add_executable(${_name}
		${SOURCES}
	)

	add_dependencies(${_name}
		${DEPENDENCIES}
	)

	target_compile_definitions(${_name} PRIVATE
		-DG_LOG_DOMAIN=\"${_name}\"
		-DEVOLUTION_ICONDIR=\"${icondir}\"
		-DEVOLUTION_LOCALEDIR=\"${LOCALE_INSTALL_DIR}\"
		-DEVOLUTION_MODULEDIR=\"${moduledir}\"
		-DEVOLUTION_SOURCE_WEBKITDATADIR=\"${CMAKE_SOURCE_DIR}/data/webkit\"
		-DEVOLUTION_TESTGIOMODULESDIR=\"${CMAKE_CURRENT_BINARY_DIR}/test-gio-modules\"
		-DEVOLUTION_TESTTOPSRCDIR=\"${CMAKE_SOURCE_DIR}\"
		-DE_DATA_SERVER_ICONDIR=\"${E_DATA_SERVER_ICONDIR}\"
		-DLIBEUTIL_COMPILATION
	)

	target_compile_options(${_name} PUBLIC
		${AUTOAR_CFLAGS}
		${ENCHANT_CFLAGS}
		${EVOLUTION_DATA_SERVER_CFLAGS}
		${GEO_CFLAGS}
		${GNOME_PLATFORM_CFLAGS}
		${GSPELL_CFLAGS}
	)

	target_include_directories(${_name} PUBLIC
		${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_CURRENT_BINARY_DIR}
		${AUTOAR_INCLUDE_DIRS}
		${ENCHANT_INCLUDE_DIRS}
		${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
		${GEO_INCLUDE_DIRS}
		${GNOME_PLATFORM_INCLUDE_DIRS}
		${GSPELL_INCLUDE_DIRS}
	)

	target_link_libraries(${_name}
		${DEPENDENCIES}
		${AUTOAR_LDFLAGS}
		${ENCHANT_LDFLAGS}
		${EVOLUTION_DATA_SERVER_LDFLAGS}
		${GEO_LDFLAGS}
		${GNOME_PLATFORM_LDFLAGS}
		${GSPELL_LDFLAGS}
		${ICONV_LIBS}
		${MATH_LDFLAGS}
	)
endmacro(add_private_program)

macro(add_private_programs_simple _name0)
	foreach(_name ${_name0} ${ARGN})
		add_private_program(${_name} ${_name}.c)
	endforeach(_name)
endmacro(add_private_programs_simple)

add_private_programs_simple(
	evolution-source-viewer
	test-accounts-window
	test-calendar
	test-category-completion
	test-contact-store
	test-dateedit
	test-html-editor
	test-mail-signatures
	test-markdown
	test-markdown-editor
	test-month-widget
	test-name-selector
	test-preferences-window
	test-proxy-preferences
	test-source-combo-box
	test-source-config
	test-source-selector
	test-tree-view-frame
	test-web-view-jsc
)

add_private_program(test-html-editor-units
	test-html-editor-units.c
	test-html-editor-units-bugs.h
	test-html-editor-units-bugs.c
	test-html-editor-units-utils.h
	test-html-editor-units-utils.c
)
add_dependencies(test-html-editor-units evolutiontestsettings)

# verify e-supported-locales-private.h contains all locales from /po directory
if(ENABLE_MAINTAINER_MODE)
	set(priv_file ${CMAKE_CURRENT_SOURCE_DIR}/e-supported-locales-private.h)
	file(GLOB po_files ${GETTEXT_PO_DIR}/*.po)
	file(READ ${priv_file} supported_locales)

	foreach(file IN LISTS po_files)
		get_filename_component(lang ${file} NAME_WE)
		string(FIND "${supported_locales}" "\"${lang}\"" found)
		if(found LESS 0)
			message(SEND_ERROR "Localization '${lang}' missing in ${priv_file}, add it with corresponding locale string")
		endif(found LESS 0)
	endforeach(file)
endif(ENABLE_MAINTAINER_MODE)

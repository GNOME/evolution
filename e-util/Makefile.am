NULL =

privsolib_LTLIBRARIES = libevolution-util.la

evolution_util_includedir = $(privincludedir)/e-util
ecpsdir = $(privdatadir)/ecps
ruledir = $(privdatadir)

ENUM_TYPES = e-util-enums.h

e-util-enumtypes.h: $(top_srcdir)/enumtypes.h.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.h.template \
		--fhead "#ifndef E_UTIL_ENUMTYPES_H\n#define E_UTIL_ENUMTYPES_H\n" \
		--ftail "#endif /* E_UTIL_ENUMTYPES_H */\n" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

e-util-enumtypes.c: $(top_srcdir)/enumtypes.c.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.c.template \
		--fhead "#include \"e-util-enumtypes.h\"" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

e-marshal.h: e-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --header --prefix=e_marshal $^ > e-marshal.h.tmp && \
	mv e-marshal.h.tmp e-marshal.h

e-marshal.c: e-marshal.list
	$(AM_V_GEN) echo "#include \"e-marshal.h\"" >> e-marshal.c.tmp && \
	$(GLIB_GENMARSHAL) --body --prefix=e_marshal $^ >> e-marshal.c.tmp && \
	mv e-marshal.c.tmp e-marshal.c

ENUM_GENERATED = e-util-enumtypes.h e-util-enumtypes.c
MARSHAL_GENERATED = e-marshal.c e-marshal.h

error_DATA = \
	e-system.error \
	filter.error \
	widgets.error \
	$(NULL)
errordir = $(privdatadir)/errors
@EVO_PLUGIN_RULE@

ui_DATA = \
	e-html-editor-manager.ui \
	e-send-options.ui \
	e-table-config.ui \
	e-timezone-dialog.ui \
	filter.ui \
	gal-view-instance-save-as-dialog.ui \
	$(NULL)

xpm_icons = \
	arrow-down.xpm \
	arrow-up.xpm \
	check-empty.xpm \
	check-filled.xpm \
	$(NULL)

noinst_PROGRAMS = \
	evolution-source-viewer \
	test-calendar \
	test-category-completion \
	test-contact-store \
	test-dateedit \
	test-html-editor \
	test-html-editor-units \
	test-mail-signatures \
	test-name-selector \
	test-preferences-window \
	test-proxy-preferences \
	test-source-combo-box \
	test-source-config \
	test-source-selector \
	test-tree-view-frame \
	$(NULL)

# This is not meant to be distributed to end users, unless test-html-editor-units
# is also distributed, but even then it can work without it
privsolibtestgiomodulesdir = $(privsolibdir)/test-gio-modules
privsolibtestgiomodules_LTLIBRARIES = libevolutiontestsettings.la

GITIGNOREFILES = libevolutiontestsettings.so

libevolutiontestsettings_la_SOURCES = \
	test-keyfile-settings-backend.h \
	test-keyfile-settings-backend.c \
	$(PLATFORM_SOURCES) \
	$(NULL)

libevolutiontestsettings_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DG_LOG_DOMAIN=\"evolution-test-settings\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(NULL)

libevolutiontestsettings_la_LDFLAGS = -no-install -module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolutiontestsettings_la_LIBADD =  \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(NULL)

libevolution_util_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DLIBEUTIL_COMPILATION \
	-DEVOLUTION_BINDIR=\""$(bindir)"\" \
	-DEVOLUTION_DATADIR=\""$(datadir)"\" \
	-DEVOLUTION_ECPSDIR=\""$(ecpsdir)"\" \
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\" \
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\" \
	-DEVOLUTION_HELPDIR=\""$(evolutionhelpdir)"\" \
	-DEVOLUTION_ICONDIR=\""$(icondir)"\" \
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\" \
	-DEVOLUTION_LIBDIR=\""$(datadir)"\" \
	-DEVOLUTION_LIBEXECDIR=\""$(privlibexecdir)"\" \
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\" \
	-DEVOLUTION_MODULEDIR=\""$(moduledir)"\" \
	-DEVOLUTION_PLUGINDIR=\""$(plugindir)"\" \
	-DEVOLUTION_PREFIX=\""$(prefix)"\" \
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\" \
	-DEVOLUTION_SOUNDDIR=\""$(soundsdir)"\" \
	-DEVOLUTION_SYSCONFDIR=\""$(sysconfdir)"\" \
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\" \
	-DEVOLUTION_UIDIR=\""$(uidir)"\" \
	-DEVOLUTION_RULEDIR=\"$(ruledir)\" \
	-DEVOLUTION_WEB_EXTENSIONS_DIR=\""$(webextensionsdir)"\" \
	-DEVOLUTION_TESTGIOMODULESDIR=\"$(privsolibtestgiomodulesdir)\" \
	-DG_LOG_DOMAIN=\"evolution-util\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(GNOME_PLATFORM_CFLAGS) \
	$(GEO_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(AUTOAR_CFLAGS) \
	$(NULL)

evolution_util_include_HEADERS =  \
	e-util.h \
	e-action-combo-box.h \
	e-activity-bar.h \
	e-activity-proxy.h \
	e-activity.h \
	e-alarm-selector.h \
	e-alert-bar.h \
	e-alert-dialog.h \
	e-alert-sink.h \
	e-alert.h \
	e-attachment-bar.h \
	e-attachment-dialog.h \
	e-attachment-handler-image.h \
	e-attachment-handler.h \
	e-attachment-icon-view.h \
	e-attachment-paned.h \
	e-attachment-store.h \
	e-attachment-tree-view.h \
	e-attachment-view.h \
	e-attachment.h \
	e-auth-combo-box.h \
	e-autocomplete-selector.h \
	e-bit-array.h \
	e-book-source-config.h \
	e-buffer-tagger.h \
	e-cal-source-config.h \
	e-calendar-item.h \
	e-calendar.h \
	e-canvas-background.h \
	e-canvas-utils.h \
	e-canvas-vbox.h \
	e-canvas.h \
	e-categories-config.h \
	e-categories-dialog.h \
	e-categories-editor.h \
	e-categories-selector.h \
	e-category-completion.h \
	e-category-editor.h \
	e-cell-checkbox.h \
	e-cell-combo.h \
	e-cell-date-edit.h \
	e-cell-date-int.h \
	e-cell-date.h \
	e-cell-hbox.h \
	e-cell-number.h \
	e-cell-percent.h \
	e-cell-pixbuf.h \
	e-cell-popup.h \
	e-cell-size.h \
	e-cell-text.h \
	e-cell-toggle.h \
	e-cell-tree.h \
	e-cell-vbox.h \
	e-cell.h \
	e-charset-combo-box.h \
	e-charset.h \
	e-client-cache.h \
	e-client-combo-box.h \
	e-client-selector.h \
	e-color-chooser-widget.h \
	e-color-combo.h \
	e-config.h \
	e-conflict-search-selector.h \
	e-contact-store.h \
	e-content-editor.h \
	e-content-request.h \
	e-data-capture.h \
	e-dateedit.h \
	e-datetime-format.h \
	e-destination-store.h \
	e-dialog-utils.h \
	e-dialog-widgets.h \
	e-emoticon-action.h \
	e-emoticon-chooser-menu.h \
	e-emoticon-chooser.h \
	e-emoticon-tool-button.h \
	e-emoticon.h \
	e-event.h \
	e-file-request.h \
	e-file-utils.h \
	e-filter-code.h \
	e-filter-color.h \
	e-filter-datespec.h \
	e-filter-element.h \
	e-filter-file.h \
	e-filter-input.h \
	e-filter-int.h \
	e-filter-option.h \
	e-filter-part.h \
	e-filter-rule.h \
	e-focus-tracker.h \
	e-html-editor-actions.h \
	e-html-editor-cell-dialog.h \
	e-html-editor-dialog.h \
	e-html-editor-find-dialog.h \
	e-html-editor-hrule-dialog.h \
	e-html-editor-image-dialog.h \
	e-html-editor-link-dialog.h \
	e-html-editor-page-dialog.h \
	e-html-editor-paragraph-dialog.h \
	e-html-editor-replace-dialog.h \
	e-html-editor-spell-check-dialog.h \
	e-html-editor-table-dialog.h \
	e-html-editor-text-dialog.h \
	e-html-editor.h \
	e-html-utils.h \
	e-icon-factory.h \
	e-image-chooser.h \
	e-image-chooser-dialog.h \
	e-import-assistant.h \
	e-import.h \
	e-interval-chooser.h \
	e-mail-identity-combo-box.h \
	e-mail-signature-combo-box.h \
	e-mail-signature-editor.h \
	e-mail-signature-manager.h \
	e-mail-signature-preview.h \
	e-mail-signature-script-dialog.h \
	e-mail-signature-tree-view.h \
	e-map.h \
	e-marshal.h \
	e-menu-tool-action.h \
	e-menu-tool-button.h \
	e-misc-utils.h \
	e-mktemp.h \
	e-name-selector-dialog.h \
	e-name-selector-entry.h \
	e-name-selector-list.h \
	e-name-selector-model.h \
	e-name-selector.h \
	e-online-button.h \
	e-paned.h \
	e-passwords.h \
	e-photo-cache.h \
	e-photo-source.h \
	e-picture-gallery.h \
	e-plugin-ui.h \
	e-plugin.h \
	e-poolv.h \
	e-popup-action.h \
	e-popup-menu.h \
	e-port-entry.h \
	e-preferences-window.h \
	e-preview-pane.h \
	e-print.h \
	e-printable.h \
	e-proxy-combo-box.h \
	e-proxy-editor.h \
	e-proxy-link-selector.h \
	e-proxy-preferences.h \
	e-proxy-selector.h \
	e-reflow-model.h \
	e-reflow.h \
	e-rule-context.h \
	e-rule-editor.h \
	e-search-bar.h \
	e-selectable.h \
	e-selection-model-array.h \
	e-selection-model-simple.h \
	e-selection-model.h \
	e-selection.h \
	e-send-options.h \
	e-simple-async-result.h \
	e-sorter-array.h \
	e-sorter.h \
	e-source-combo-box.h \
	e-source-config-backend.h \
	e-source-config-dialog.h \
	e-source-config.h \
	e-source-conflict-search.h \
	e-source-selector-dialog.h \
	e-source-selector.h \
	e-source-util.h \
	e-spell-checker.h \
	e-spell-dictionary.h \
	e-spell-entry.h \
	e-spell-text-view.h \
	e-spinner.h \
	e-stock-request.h \
	e-table-click-to-add.h \
	e-table-col-dnd.h \
	e-table-col.h \
	e-table-column-selector.h \
	e-table-column-specification.h \
	e-table-config.h \
	e-table-defines.h \
	e-table-extras.h \
	e-table-field-chooser-dialog.h \
	e-table-field-chooser-item.h \
	e-table-field-chooser.h \
	e-table-group-container.h \
	e-table-group-leaf.h \
	e-table-group.h \
	e-table-header-item.h \
	e-table-header-utils.h \
	e-table-header.h \
	e-table-item.h \
	e-table-model.h \
	e-table-one.h \
	e-table-search.h \
	e-table-selection-model.h \
	e-table-sort-info.h \
	e-table-sorted-variable.h \
	e-table-sorted.h \
	e-table-sorter.h \
	e-table-sorting-utils.h \
	e-table-specification.h \
	e-table-state.h \
	e-table-subset-variable.h \
	e-table-subset.h \
	e-table-utils.h \
	e-table.h \
	e-text-event-processor-emacs-like.h \
	e-text-event-processor-types.h \
	e-text-event-processor.h \
	e-text-model-repos.h \
	e-text-model.h \
	e-text.h \
	e-timezone-dialog.h \
	e-tree-model-generator.h \
	e-tree-model.h \
	e-tree-selection-model.h \
	e-tree-table-adapter.h \
	e-tree-view-frame.h \
	e-tree.h \
	e-unicode.h \
	e-url-entry.h \
	e-util-enums.h \
	e-util-enumtypes.h \
	e-web-view-preview.h \
	e-web-view.h \
	e-widget-undo.h \
	e-xml-utils.h \
	ea-calendar-cell.h \
	ea-calendar-item.h \
	ea-cell-table.h \
	ea-factory.h \
	ea-widgets.h \
	gal-a11y-e-cell-popup.h \
	gal-a11y-e-cell-registry.h \
	gal-a11y-e-cell-text.h \
	gal-a11y-e-cell-toggle.h \
	gal-a11y-e-cell-tree.h \
	gal-a11y-e-cell-vbox.h \
	gal-a11y-e-cell.h \
	gal-a11y-e-table-click-to-add-factory.h \
	gal-a11y-e-table-click-to-add.h \
	gal-a11y-e-table-column-header.h \
	gal-a11y-e-table-factory.h \
	gal-a11y-e-table-item-factory.h \
	gal-a11y-e-table-item.h \
	gal-a11y-e-table.h \
	gal-a11y-e-text-factory.h \
	gal-a11y-e-text.h \
	gal-a11y-e-tree-factory.h \
	gal-a11y-e-tree.h \
	gal-a11y-factory.h \
	gal-a11y-util.h \
	gal-view-collection.h \
	gal-view-etable.h \
	gal-view-instance-save-as-dialog.h \
	gal-view-instance.h \
	gal-view.h \
	$(NULL)

if OS_WIN32
PLATFORM_SOURCES = \
	e-win32-reloc.c \
	e-win32-defaults.c \
	e-win32-defaults.h \
	$(NULL)
endif

libevolution_util_la_SOURCES = \
	$(evolution_util_include_HEADERS) \
	e-action-combo-box.c \
	e-activity-bar.c \
	e-activity-proxy.c \
	e-activity.c \
	e-alarm-selector.c \
	e-alert-bar.c \
	e-alert-dialog.c \
	e-alert-sink.c \
	e-alert.c \
	e-attachment-bar.c \
	e-attachment-dialog.c \
	e-attachment-handler-image.c \
	e-attachment-handler.c \
	e-attachment-icon-view.c \
	e-attachment-paned.c \
	e-attachment-store.c \
	e-attachment-tree-view.c \
	e-attachment-view.c \
	e-attachment.c \
	e-auth-combo-box.c \
	e-autocomplete-selector.c \
	e-bit-array.c \
	e-book-source-config.c \
	e-buffer-tagger.c \
	e-cal-source-config.c \
	e-calendar-item.c \
	e-calendar.c \
	e-canvas-background.c \
	e-canvas-utils.c \
	e-canvas-vbox.c \
	e-canvas.c \
	e-categories-config.c \
	e-categories-dialog.c \
	e-categories-editor.c \
	e-categories-selector.c \
	e-category-completion.c \
	e-category-editor.c \
	e-cell-checkbox.c \
	e-cell-combo.c \
	e-cell-date-edit.c \
	e-cell-date-int.c \
	e-cell-date.c \
	e-cell-hbox.c \
	e-cell-number.c \
	e-cell-percent.c \
	e-cell-pixbuf.c \
	e-cell-popup.c \
	e-cell-size.c \
	e-cell-text.c \
	e-cell-toggle.c \
	e-cell-tree.c \
	e-cell-vbox.c \
	e-cell.c \
	e-charset-combo-box.c \
	e-charset.c \
	e-client-cache.c \
	e-client-combo-box.c \
	e-client-selector.c \
	e-color-chooser-widget.c \
	e-color-combo.c \
	e-config.c \
	e-conflict-search-selector.c \
	e-contact-store.c \
	e-content-editor.c \
	e-content-request.c \
	e-data-capture.c \
	e-dateedit.c \
	e-datetime-format.c \
	e-destination-store.c \
	e-dialog-utils.c \
	e-dialog-widgets.c \
	e-emoticon-action.c \
	e-emoticon-chooser-menu.c \
	e-emoticon-chooser.c \
	e-emoticon-tool-button.c \
	e-emoticon.c \
	e-event.c \
	e-file-request.c \
	e-file-utils.c \
	e-filter-code.c \
	e-filter-color.c \
	e-filter-datespec.c \
	e-filter-element.c \
	e-filter-file.c \
	e-filter-input.c \
	e-filter-int.c \
	e-filter-option.c \
	e-filter-part.c \
	e-filter-rule.c \
	e-focus-tracker.c \
	e-html-editor-actions.c \
	e-html-editor-cell-dialog.c \
	e-html-editor-dialog.c \
	e-html-editor-find-dialog.c \
	e-html-editor-hrule-dialog.c \
	e-html-editor-image-dialog.c \
	e-html-editor-link-dialog.c \
	e-html-editor-page-dialog.c \
	e-html-editor-paragraph-dialog.c \
	e-html-editor-private.h \
	e-html-editor-replace-dialog.c \
	e-html-editor-spell-check-dialog.c \
	e-html-editor-table-dialog.c \
	e-html-editor-text-dialog.c \
	e-html-editor.c \
	e-html-utils.c \
	e-icon-factory.c \
	e-image-chooser.c \
	e-image-chooser-dialog.c \
	e-import-assistant.c \
	e-import.c \
	e-interval-chooser.c \
	e-mail-identity-combo-box.c \
	e-mail-signature-combo-box.c \
	e-mail-signature-editor.c \
	e-mail-signature-manager.c \
	e-mail-signature-preview.c \
	e-mail-signature-script-dialog.c \
	e-mail-signature-tree-view.c \
	e-map.c \
	e-marshal.c \
	e-menu-tool-action.c \
	e-menu-tool-button.c \
	e-misc-utils.c \
	e-mktemp.c \
	e-name-selector-dialog.c \
	e-name-selector-entry.c \
	e-name-selector-list.c \
	e-name-selector-model.c \
	e-name-selector.c \
	e-online-button.c \
	e-paned.c \
	e-passwords.c \
	e-photo-cache.c \
	e-photo-source.c \
	e-picture-gallery.c \
	e-plugin-ui.c \
	e-plugin.c \
	e-poolv.c \
	e-popup-action.c \
	e-popup-menu.c \
	e-port-entry.c \
	e-preferences-window.c \
	e-preview-pane.c \
	e-print.c \
	e-printable.c \
	e-proxy-combo-box.c \
	e-proxy-editor.c \
	e-proxy-link-selector.c \
	e-proxy-preferences.c \
	e-proxy-selector.c \
	e-reflow-model.c \
	e-reflow.c \
	e-rule-context.c \
	e-rule-editor.c \
	e-search-bar.c \
	e-selectable.c \
	e-selection-model-array.c \
	e-selection-model-simple.c \
	e-selection-model.c \
	e-selection.c \
	e-send-options.c \
	e-simple-async-result.c \
	e-sorter-array.c \
	e-sorter.c \
	e-source-combo-box.c \
	e-source-config-backend.c \
	e-source-config-dialog.c \
	e-source-config.c \
	e-source-conflict-search.c \
	e-source-selector-dialog.c \
	e-source-selector.c \
	e-source-util.c \
	e-spell-checker.c \
	e-spell-dictionary.c \
	e-spell-entry.c \
	e-spell-text-view.c \
	e-spinner.c \
	e-stock-request.c \
	e-table-click-to-add.c \
	e-table-col.c \
	e-table-column-selector.c \
	e-table-column-specification.c \
	e-table-config.c \
	e-table-extras.c \
	e-table-field-chooser-dialog.c \
	e-table-field-chooser-item.c \
	e-table-field-chooser.c \
	e-table-group-container.c \
	e-table-group-leaf.c \
	e-table-group.c \
	e-table-header-item.c \
	e-table-header-utils.c \
	e-table-header.c \
	e-table-item.c \
	e-table-model.c \
	e-table-one.c \
	e-table-search.c \
	e-table-selection-model.c \
	e-table-sort-info.c \
	e-table-sorted-variable.c \
	e-table-sorted.c \
	e-table-sorter.c \
	e-table-sorting-utils.c \
	e-table-specification.c \
	e-table-state.c \
	e-table-subset-variable.c \
	e-table-subset.c \
	e-table-utils.c \
	e-table.c \
	e-text-event-processor-emacs-like.c \
	e-text-event-processor.c \
	e-text-model-repos.c \
	e-text-model.c \
	e-text.c \
	e-timezone-dialog.c \
	e-tree-model-generator.c \
	e-tree-model.c \
	e-tree-selection-model.c \
	e-tree-table-adapter.c \
	e-tree-view-frame.c \
	e-tree.c \
	e-unicode.c \
	e-url-entry.c \
	e-util-enumtypes.c \
	e-util-private.h \
	e-web-view-preview.c \
	e-web-view.c \
	e-widget-undo.c \
	e-xml-utils.c \
	ea-calendar-cell.c \
	ea-calendar-item.c \
	ea-cell-table.c \
	ea-widgets.c \
	gal-a11y-e-cell-popup.c \
	gal-a11y-e-cell-registry.c \
	gal-a11y-e-cell-text.c \
	gal-a11y-e-cell-toggle.c \
	gal-a11y-e-cell-tree.c \
	gal-a11y-e-cell-vbox.c \
	gal-a11y-e-cell.c \
	gal-a11y-e-table-click-to-add-factory.c \
	gal-a11y-e-table-click-to-add.c \
	gal-a11y-e-table-column-header.c \
	gal-a11y-e-table-factory.c \
	gal-a11y-e-table-item-factory.c \
	gal-a11y-e-table-item.c \
	gal-a11y-e-table.c \
	gal-a11y-e-text-factory.c \
	gal-a11y-e-text.c \
	gal-a11y-e-tree-factory.c \
	gal-a11y-e-tree.c \
	gal-a11y-util.c \
	gal-view-collection.c \
	gal-view-etable.c \
	gal-view-instance-save-as-dialog.c \
	gal-view-instance.c \
	gal-view.c \
	$(PLATFORM_SOURCES) \
	$(NULL)

libevolution_util_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

libevolution_util_la_LIBADD =  \
	$(top_builddir)/libgnomecanvas/libgnomecanvas.la \
	$(ICONV_LIBS) \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(GNOME_PLATFORM_LIBS) \
	$(GEO_LIBS) \
	$(ENCHANT_LIBS) \
	$(GTKSPELL_LIBS) \
	$(AUTOAR_LIBS) \
	$(INTLLIBS) \
	$(MATH_LIB) \
	$(NULL)

TEST_CPPFLAGS = \
	$(libevolution_util_la_CPPFLAGS) \
	$(NULL)

TEST_LDADD = \
	libevolution-util.la \
	$(libevolution_util_la_LIBADD) \
	$(NULL)

evolution_source_viewer_CPPFLAGS = $(TEST_CPPFLAGS)
evolution_source_viewer_SOURCES = evolution-source-viewer.c
evolution_source_viewer_LDADD = $(TEST_LDADD)

test_calendar_CPPFLAGS = $(TEST_CPPFLAGS)
test_calendar_SOURCES = test-calendar.c
test_calendar_LDADD = $(TEST_LDADD)

test_category_completion_CPPFLAGS = $(TEST_CPPFLAGS)
test_category_completion_SOURCES = test-category-completion.c
test_category_completion_LDADD = $(TEST_LDADD)

test_contact_store_CPPFLAGS = $(TEST_CPPFLAGS)
test_contact_store_SOURCES = test-contact-store.c
test_contact_store_LDADD = $(TEST_LDADD)

test_dateedit_CPPFLAGS = $(TEST_CPPFLAGS)
test_dateedit_SOURCES = test-dateedit.c
test_dateedit_LDADD = $(TEST_LDADD)

test_html_editor_CPPFLAGS = $(TEST_CPPFLAGS)
test_html_editor_SOURCES = test-html-editor.c
test_html_editor_LDADD = $(TEST_LDADD)

test_html_editor_units_CPPFLAGS = $(TEST_CPPFLAGS) -DTEST_TOP_SRCDIR=\""$(top_srcdir)"\"
test_html_editor_units_SOURCES = \
	test-html-editor-units-bugs.h \
	test-html-editor-units-bugs.c \
	test-html-editor-units-utils.h \
	test-html-editor-units-utils.c \
	test-html-editor-units.c \
	$(NULL)
test_html_editor_units_LDADD = $(TEST_LDADD)

test_mail_signatures_CPPFLAGS = $(TEST_CPPFLAGS)
test_mail_signatures_SOURCES = test-mail-signatures.c
test_mail_signatures_LDADD = $(TEST_LDADD)

test_name_selector_CPPFLAGS = $(TEST_CPPFLAGS)
test_name_selector_SOURCES = test-name-selector.c
test_name_selector_LDADD = $(TEST_LDADD)

test_preferences_window_CPPFLAGS = $(TEST_CPPFLAGS)
test_preferences_window_SOURCES = test-preferences-window.c
test_preferences_window_LDADD = $(TEST_LDADD)

test_proxy_preferences_CPPFLAGS = $(TEST_CPPFLAGS)
test_proxy_preferences_SOURCES = test-proxy-preferences.c
test_proxy_preferences_LDADD = $(TEST_LDADD)

test_source_combo_box_CPPFLAGS = $(TEST_CPPFLAGS)
test_source_combo_box_SOURCES = test-source-combo-box.c
test_source_combo_box_LDADD = $(TEST_LDADD)

test_source_config_CPPFLAGS = $(TEST_CPPFLAGS)
test_source_config_SOURCES = test-source-config.c
test_source_config_LDADD = $(TEST_LDADD)

test_source_selector_CPPFLAGS = $(TEST_CPPFLAGS)
test_source_selector_SOURCES = test-source-selector.c
test_source_selector_LDADD = $(TEST_LDADD)

test_tree_view_frame_CPPFLAGS = $(TEST_CPPFLAGS)
test_tree_view_frame_SOURCES = test-tree-view-frame.c
test_tree_view_frame_LDADD = $(TEST_LDADD)

EXTRA_DIST = \
	e-system.error.xml \
	filter.error.xml \
	widgets.error.xml \
	e-marshal.list \
	$(ui_DATA) \
	$(xpm_icons) \
	$(NULL)

BUILT_SOURCES = \
	$(ENUM_GENERATED) \
	$(MARSHAL_GENERATED) \
	$(error_DATA) \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

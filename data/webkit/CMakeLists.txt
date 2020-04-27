set(DATA_FILES
	e-convert.js
	e-editor.js
	e-selection.js
	e-undo-redo.js
	e-web-view.js
	webview.css
	webview-print.css
)

install(FILES ${DATA_FILES}
	DESTINATION ${webkitdatadir}
)

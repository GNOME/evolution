set(DATA_FILES
	e-convert.js
	e-web-view.js
	webview.css
	webview-print.css
)

install(FILES ${DATA_FILES}
	DESTINATION ${webkitdatadir}
)

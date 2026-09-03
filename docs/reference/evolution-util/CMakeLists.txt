set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/e-util
)

set(DEPENDENCIES
	email-engine
	evolution-util
	evolution-shell
)

set(IGNORE_HEADERS
	e-html-editor-actions.h
	e-html-editor-private.h
	e-marshal.h
	e-util-enumtypes.h
	e-util-private.h
	ea-calendar-cell.h
	ea-calendar-item.h
	ea-cell-table.h
	ea-factory.h
	ea-widgets.h
	gal-a11y-e-text-factory.h
	gal-a11y-e-text.h
	gal-a11y-factory.h
	gal-a11y-util.h
	test-html-editor-units-bugs.h
	test-html-editor-units-utils.h
	test-keyfile-settings-backend.h
)

add_gtkdoc(evolution-util e-util "EVO_DISABLE_DEPRECATED" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

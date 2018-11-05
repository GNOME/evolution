set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/shell
)

set(DEPENDENCIES
	email-engine
	evolution-util
	evolution-shell
	gtkdoc-evolution-mail-engine
	gtkdoc-evolution-util
)

set(IGNORE_HEADERS
	evo-version.h
	e-shell-window-private.h
	e-shell-enumtypes.h
)

add_gtkdoc(evolution-shell shell "EVO_DISABLE_DEPRECATED" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

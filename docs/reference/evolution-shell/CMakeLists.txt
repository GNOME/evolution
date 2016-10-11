set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/shell
)

set(DEPENDENCIES
	email-engine
	evolution-util
	evolution-shell
)

set(IGNORE_HEADERS
	evo-version.h
	e-shell-window-private.h
	e-shell-enumtypes.h
)

add_gtkdoc(evolution-shell shell "" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

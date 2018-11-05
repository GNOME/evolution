set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/em-format
)

set(DEPENDENCIES
	evolution-mail-formatter
	gtkdoc-evolution-shell
)

set(IGNORE_HEADERS
	e-mail-formatter-enumtypes.h
)

add_gtkdoc(evolution-mail-formatter em-format "EVO_DISABLE_DEPRECATED" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

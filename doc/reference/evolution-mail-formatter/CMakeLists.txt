set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/em-format
)

set(DEPENDENCIES
	evolution-mail-formatter
)

set(IGNORE_HEADERS
	e-mail-formatter-enumtypes.h
)

add_gtkdoc(evolution-mail-formatter em-format "" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

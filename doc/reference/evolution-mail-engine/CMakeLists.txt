set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/libemail-engine
)

set(DEPENDENCIES
	email-engine
)

set(IGNORE_HEADERS
	e-mail-engine-enumtypes.h
)

add_gtkdoc(evolution-mail-engine email-engine "" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/libemail-engine
)

set(DEPENDENCIES
	email-engine
	gtkdoc-evolution-util
)

set(IGNORE_HEADERS
	e-mail-engine-enumtypes.h
)

add_gtkdoc(evolution-mail-engine email-engine "EVO_DISABLE_DEPRECATED" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

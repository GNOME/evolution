set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/composer
)

set(DEPENDENCIES
	evolution-mail-composer
)

set(IGNORE_HEADERS
	e-composer-actions.h
	e-composer-private.h
)

add_gtkdoc(evolution-mail-composer composer "" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

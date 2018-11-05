set(SOURCEDIRS
	${CMAKE_SOURCE_DIR}/src/composer
)

set(DEPENDENCIES
	evolution-mail-composer
	gtkdoc-evolution-mail-engine
	gtkdoc-evolution-mail-formatter
	gtkdoc-evolution-shell
	gtkdoc-evolution-util
)

set(IGNORE_HEADERS
	e-composer-actions.h
	e-composer-private.h
)

add_gtkdoc(evolution-mail-composer composer "EVO_DISABLE_DEPRECATED" SOURCEDIRS DEPENDENCIES IGNORE_HEADERS)

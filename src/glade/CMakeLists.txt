pkg_check_variable(glade_prefix gladeui-2.0 prefix)
pkg_check_variable(glade_catalogdir gladeui-2.0 catalogdir)
pkg_check_variable(glade_moduledir gladeui-2.0 moduledir)

# To honor configured prefix, but still use the right place
string(REPLACE "${glade_prefix}" "${CMAKE_INSTALL_PREFIX}" glade_catalogdir "${glade_catalogdir}")
string(REPLACE "${glade_prefix}" "${CMAKE_INSTALL_PREFIX}" glade_moduledir "${glade_moduledir}")

install(FILES evolution.xml
	DESTINATION ${glade_catalogdir}
)

set(DEPENDENCIES
	evolution-calendar
	evolution-mail
	evolution-util
)

add_library(gladeevolution MODULE
	gladeevolution.c
)

add_dependencies(gladeevolution
	${DEPENDENCIES}
)

target_compile_definitions(gladeevolution PRIVATE
	-DG_LOG_DOMAIN=\"gladeevolution\"
)

target_compile_options(gladeevolution PUBLIC
	${EVOLUTION_DATA_SERVER_CFLAGS}
	${GNOME_PLATFORM_CFLAGS}
)

target_include_directories(gladeevolution PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(gladeevolution
	${DEPENDENCIES}
	${EVOLUTION_DATA_SERVER_LDFLAGS}
	${GNOME_PLATFORM_LDFLAGS}
)

install(TARGETS gladeevolution
	DESTINATION ${glade_moduledir}
)

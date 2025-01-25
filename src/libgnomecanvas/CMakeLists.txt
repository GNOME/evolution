set(SOURCES
	gailcanvas.c
	gailcanvasgroup.c
	gailcanvasgroupfactory.c
	gailcanvasitem.c
	gailcanvasitemfactory.c
	gailcanvaswidget.c
	gailcanvaswidgetfactory.c
	gnome-canvas-pixbuf.c
	gnome-canvas-rect.c
	gnome-canvas-util.c
	gnome-canvas-widget.c
	gnome-canvas.c
)

set(HEADERS
	gailcanvas.h
	gailcanvasgroup.h
	gailcanvasgroupfactory.h
	gailcanvasitem.h
	gailcanvasitemfactory.h
	gailcanvaswidget.h
	gailcanvaswidgetfactory.h
	gnome-canvas-pixbuf.h
	gnome-canvas-rect.h
	gnome-canvas-util.h
	gnome-canvas-widget.h
	gnome-canvas.h
	libgnomecanvas.h
)

add_library(gnomecanvas SHARED
	${SOURCES}
	${HEADERS}
)

target_compile_definitions(gnomecanvas PRIVATE
	-DG_LOG_DOMAIN=\"GnomeCanvas\"
)

target_compile_options(gnomecanvas PUBLIC
	${GNOME_PLATFORM_CFLAGS}
)

set_target_properties(gnomecanvas PROPERTIES
	INSTALL_RPATH ""
)

target_include_directories(gnomecanvas PUBLIC
	${CMAKE_BINARY_DIR}
	${CMAKE_BINARY_DIR}/src
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${GNOME_PLATFORM_INCLUDE_DIRS}
)

target_link_libraries(gnomecanvas
	${GNOME_PLATFORM_LDFLAGS}
	${MATH_LDFLAGS}
)

install(TARGETS gnomecanvas
	DESTINATION ${privsolibdir}
)

install(FILES ${HEADERS}
	DESTINATION ${privincludedir}/libgnomecanvas
)

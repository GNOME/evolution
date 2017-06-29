# GtkDoc.cmake
#
# Macros to support develper documentation build from sources with gtk-doc.
#
# add_gtkdoc(_module _namespace _deprecated_guards _srcdirsvar _depsvar _ignoreheadersvar)
#    Adds rules to build developer documentation using gtk-doc for some part.
#    Arguments:
#       _module - the module name, like 'camel'; it expects ${_part}-docs.sgml.in in the CMAKE_CURRENT_SOURCE_DIR
#       _namespace - namespace for symbols
#       _deprecated_guards - define name, which guards deprecated symbols
#       _srcdirsvar - variable with dirs where the source files are located
#       _depsvar - a variable with dependencies (targets)
#       _ignoreheadersvar - a variable with a set of header files to ignore
#
# It also adds custom target gtkdoc-rebuild-${_module}-sgml to rebuild the sgml.in
# file based on the current sources.

include(PrintableOptions)

add_printable_option(ENABLE_GTK_DOC "Use gtk-doc to build documentation" OFF)

if(NOT ENABLE_GTK_DOC)
	return()
endif(NOT ENABLE_GTK_DOC)

find_program(GTKDOC_SCAN gtkdoc-scan)
find_program(GTKDOC_MKDB gtkdoc-mkdb)
find_program(GTKDOC_MKHTML gtkdoc-mkhtml)
find_program(GTKDOC_FIXXREF gtkdoc-fixxref)

if(NOT (GTKDOC_SCAN AND GTKDOC_MKDB AND GTKDOC_MKHTML AND GTKDOC_FIXXREF))
	message(FATAL_ERROR "Cannot find all gtk-doc binaries, install them or use -DENABLE_GTK_DOC=OFF instead")
	return()
endif()

if(NOT TARGET gtkdocs)
	add_custom_target(gtkdocs ALL)
endif(NOT TARGET gtkdocs)

if(NOT TARGET gtkdoc-rebuild-sgmls)
	add_custom_target(gtkdoc-rebuild-sgmls)
endif(NOT TARGET gtkdoc-rebuild-sgmls)

macro(add_gtkdoc _module _namespace _deprecated_guards _srcdirsvar _depsvar _ignoreheadersvar)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/${_module}-docs.sgml.in
		${CMAKE_CURRENT_BINARY_DIR}/${_module}-docs.sgml
		@ONLY
	)

	set(OUTPUT_DOCDIR ${SHARE_INSTALL_PREFIX}/gtk-doc/html/${_module})

	set(_filedeps)
	set(_srcdirs)
	foreach(_srcdir ${${_srcdirsvar}})
		set(_srcdirs ${_srcdirs} --source-dir="${_srcdir}")
		file(GLOB _files ${_srcdir}/*.h* ${_srcdir}/*.c*)
		list(APPEND _filedeps ${_files})
	endforeach(_srcdir)

	set(_mkhtml_prefix "")
	if(APPLE)
		set(_mkhtml_prefix "${CMAKE_COMMAND} -E env XML_CATALOG_FILES=\"/usr/local/etc/xml/catalog\"")
	endif(APPLE)

	add_custom_command(OUTPUT html/index.html
		COMMAND ${GTKDOC_SCAN}
			--module=${_module}
			--deprecated-guards="${_deprecated_guards}"
			--ignore-headers="${${_ignoreheadersvar}}"
			--rebuild-sections
			--rebuild-types
			${_srcdirs}

		COMMAND ${GTKDOC_MKDB}
			--module=${_module}
			--name-space=${_namespace}
			--main-sgml-file="${CMAKE_CURRENT_BINARY_DIR}/${_module}-docs.sgml"
			--sgml-mode
			--output-format=xml
			${_srcdirs}

		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/html"

		COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}/html" ${_mkhtml_prefix} ${GTKDOC_MKHTML} --path=.. ${_module} ../${_module}-docs.sgml

		COMMAND ${GTKDOC_FIXXREF}
			--module=${_module}
			--module-dir=html
			--extra-dir=..
			--html-dir="${OUTPUT_DOCDIR}"

		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${_module}-docs.sgml"
			${_filedeps}
		COMMENT "Generating ${_module} documentation"
	)

	add_custom_target(gtkdoc-${_module}
		DEPENDS html/index.html
	)

	if(${_depsvar})
		add_dependencies(gtkdoc-${_module} ${${_depsvar}})
	endif(${_depsvar})

	add_dependencies(gtkdocs gtkdoc-${_module})

	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
		DESTINATION ${OUTPUT_DOCDIR}
	)

	# ***************************************
	# sgml.in file rebuild, unconditional
	# ***************************************
	add_custom_target(gtkdoc-rebuild-${_module}-sgml
		COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/tmp"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/tmp"

		COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}/tmp"
			${GTKDOC_SCAN}
			--module=${_module}
			--deprecated-guards="${_deprecated_guards}"
			--ignore-headers="${_ignore_headers}"
			--rebuild-sections
			--rebuild-types
			${_srcdirs}

		COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}/tmp"
			${GTKDOC_MKDB}
			--module=${_module}
			--name-space=${_namespace}
			--main-sgml-file="${CMAKE_CURRENT_BINARY_DIR}/tmp/${_module}-docs.sgml"
			--sgml-mode
			--output-format=xml
			${_srcdirs}

		COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/tmp/${_module}-docs.sgml ${CMAKE_CURRENT_SOURCE_DIR}/${_module}-docs.sgml.in

		COMMAND ${CMAKE_COMMAND} -E echo "File '${CMAKE_CURRENT_SOURCE_DIR}/${_module}-docs.sgml.in' overwritten, make sure you replace generated strings with proper content before committing."
	)

	add_dependencies(gtkdoc-rebuild-sgmls gtkdoc-rebuild-${_module}-sgml)

endmacro(add_gtkdoc)

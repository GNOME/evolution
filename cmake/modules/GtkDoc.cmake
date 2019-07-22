# GtkDoc.cmake
#
# Macros to support develper documentation build from sources with gtk-doc.
#
# Note that every target and dependency should be defined before the macro is
# called, because it uses information from those targets.
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
find_program(GTKDOC_SCANGOBJ gtkdoc-scangobj)
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

	set(_scangobj_deps)
	set(_scangobj_cflags_list)
	set(_scangobj_cflags "")
	set(_scangobj_ldflags "")
	set(_scangobj_ld_lib_dirs "")

	foreach(opt IN LISTS ${_depsvar})
		if(TARGET ${opt})
			set(_target_type)
			get_target_property(_target_type ${opt} TYPE)
			if((_target_type STREQUAL "STATIC_LIBRARY") OR (_target_type STREQUAL "SHARED_LIBRARY") OR (_target_type STREQUAL "MODULE_LIBRARY"))
				set(_compile_options)
				set(_link_libraries)

				get_target_property(_compile_options ${opt} COMPILE_OPTIONS)
				get_target_property(_link_libraries ${opt} LINK_LIBRARIES)

				list(APPEND _scangobj_cflags_list ${_compile_options})
				list(APPEND _scangobj_deps ${_link_libraries})

				unset(_compile_options)
				unset(_link_libraries)
			endif((_target_type STREQUAL "STATIC_LIBRARY") OR (_target_type STREQUAL "SHARED_LIBRARY") OR (_target_type STREQUAL "MODULE_LIBRARY"))
			unset(_target_type)
		endif(TARGET ${opt})

		list(APPEND _scangobj_deps ${opt})
	endforeach(opt)

	# Add it as the last, thus in-tree headers have precedence
	list(APPEND _scangobj_cflags_list -I${INCLUDE_INSTALL_DIR})

	if(_scangobj_deps)
		list(REMOVE_DUPLICATES _scangobj_deps)
	endif(_scangobj_deps)
	if(_scangobj_cflags_list)
		list(REMOVE_DUPLICATES _scangobj_cflags_list)
	endif(_scangobj_cflags_list)

	foreach(opt IN LISTS _scangobj_cflags_list)
		set(_scangobj_cflags "${_scangobj_cflags} ${opt}")
	endforeach(opt)

	# first add target dependencies, to have built libraries first, then add those non-target dependencies
	foreach(opt IN LISTS _scangobj_deps)
		if(TARGET ${opt})
			set(_target_type)
			get_target_property(_target_type ${opt} TYPE)
			if((_target_type STREQUAL "STATIC_LIBRARY") OR (_target_type STREQUAL "SHARED_LIBRARY") OR (_target_type STREQUAL "MODULE_LIBRARY"))
				set(_output_name "")
				get_target_property(_output_name ${opt} OUTPUT_NAME)
				if(NOT _output_name)
					set(_output_name ${opt})
				endif(NOT _output_name)
				set(_scangobj_ldflags "${_scangobj_ldflags} -L$<TARGET_FILE_DIR:${opt}> -l${_output_name}")

				if(_target_type STREQUAL "SHARED_LIBRARY" OR (_target_type STREQUAL "MODULE_LIBRARY"))
					set(_scangobj_ld_lib_dirs "${_scangobj_ld_lib_dirs}:$<TARGET_FILE_DIR:${opt}>")
				endif(_target_type STREQUAL "SHARED_LIBRARY" OR (_target_type STREQUAL "MODULE_LIBRARY"))
				unset(_output_name)
			endif((_target_type STREQUAL "STATIC_LIBRARY") OR (_target_type STREQUAL "SHARED_LIBRARY") OR (_target_type STREQUAL "MODULE_LIBRARY"))
			unset(_target_type)
		endif(TARGET ${opt})
	endforeach(opt)

	# Add extra flags from LDFLAGS environment variable
	set(_scangobj_ldflags "${_scangobj_ldflags} ${CMAKE_SHARED_LINKER_FLAGS}")

	foreach(opt IN LISTS _scangobj_deps)
		if(NOT TARGET ${opt})
			set(_scangobj_ldflags "${_scangobj_ldflags} ${opt}")
		endif(NOT TARGET ${opt})
	endforeach(opt)

	# Add it as the last, thus in-tree libs have precedence
	set(_scangobj_ldflags "${_scangobj_ldflags} -L${LIB_INSTALL_DIR}")

	set(_scangobj_prefix ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH="${_scangobj_ld_lib_dirs}:${LIB_INSTALL_DIR}:$ENV{LD_LIBRARY_PATH}")

	if(NOT (_scangobj_cflags STREQUAL ""))
		set(_scangobj_cflags --cflags "${_scangobj_cflags}")
	endif(NOT (_scangobj_cflags STREQUAL ""))

	if(NOT (_scangobj_ldflags STREQUAL ""))
		set(_scangobj_ldflags --ldflags "${_scangobj_ldflags}")
	endif(NOT (_scangobj_ldflags STREQUAL ""))

	add_custom_command(OUTPUT html/index.html
		COMMAND ${GTKDOC_SCAN}
			--module=${_module}
			--deprecated-guards="${_deprecated_guards}"
			--ignore-headers="${${_ignoreheadersvar}}"
			--rebuild-sections
			--rebuild-types
			${_srcdirs}

		COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}" ${_scangobj_prefix} ${GTKDOC_SCANGOBJ}
			--module=${_module}
			${_scangobj_cflags}
			${_scangobj_ldflags}

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

		COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}" ${_scangobj_prefix} ${GTKDOC_SCANGOBJ}
			--module=${_module}
			${_scangobj_cflags}
			${_scangobj_ldflags}

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

	unset(_scangobj_prefix)
	unset(_scangobj_deps)
	unset(_scangobj_cflags_list)
	unset(_scangobj_cflags)
	unset(_scangobj_ldflags)
	unset(_scangobj_ld_lib_dirs)
endmacro(add_gtkdoc)

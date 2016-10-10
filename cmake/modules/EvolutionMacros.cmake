# EvolutionMacros.cmake
#
# Utility macros for evolution-related files
#
# add_error_files(_part _file0)
#    Adds build and install rules to create .error files from .error.xml
#    files in the current source directory. The _file0 is expected to be
#    without the .xml extension. The macro can receive one or more error
#    files. There is created a custom "${_part}-error-files" target.
#
# add_eplug_file(_part _eplug_filename)
#    Adds build and install rules to create .eplug files from .eplug.xml
#    files in the current source directory. The _eplug_filename is expected
#    to be without the .xml extension. The macro can receive exactly one
#    eplug file. There is created a custom "${_part}-eplug-file" target.

include(FindIntltool)

macro(add_custom_xml_files _part _destination _targetsuffix _ext _mergeparam _file0)
	set(filedeps)

	foreach(file ${_file0} ${ARGN})
		intltool_merge(${file}${_ext} ${file} --xml-style --utf8 ${_mergeparam})

		get_filename_component(_path ${file} DIRECTORY)
		if(_path STREQUAL "")
			set(builtfile ${CMAKE_CURRENT_BINARY_DIR}/${file})
		else(_path STREQUAL "")
			set(builtfile ${file})
		endif(_path STREQUAL "")

		install(FILES ${builtfile}
			DESTINATION ${_destination}
		)

		list(APPEND filedeps ${builtfile})
	endforeach(file)

	add_custom_target(${_part}-${_targetsuffix}-files ALL
		DEPENDS ${filedeps}
	)
endmacro(add_custom_xml_files)

macro(add_error_files _part _file0)
	add_custom_xml_files(${_part} ${errordir} error .xml --no-translations ${_file0} ${ARGN})
endmacro(add_error_files)

macro(add_eplug_file _part _eplug_filename)
	set(PLUGINDIR "${plugindir}")
	set(SOEXT "${CMAKE_SHARED_MODULE_SUFFIX}")
	set(LOCALEDIR "${LOCALE_INSTALL_DIR}")

	configure_file(${_eplug_filename}.xml
		${CMAKE_CURRENT_BINARY_DIR}/${_eplug_filename}.in
		@ONLY
	)

	unset(PLUGINDIR)
	unset(SOEXT)
	unset(LOCALEDIR)

	add_custom_xml_files(${_part} ${plugindir} plugin .in --no-translations ${CMAKE_CURRENT_BINARY_DIR}/${_eplug_filename})
endmacro(add_eplug_file)

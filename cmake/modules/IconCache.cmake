# IconCache.cmake
#
# This is required here only to have defined target 'uninstall'
# in the same directory.
#
# Macros:
# add_icon_cache_files(_destdir _fileslistvar ...)
#    adds rules to install icons to icon cache directory with prefix _destdir;
#    the other arguments are one or more list variables with file names.

include(UninstallTarget)

macro(get_one_icon_component _instring _outvalue _outrest)
	string(FIND "${_instring}" "_" _pos)
	if(_pos EQUAL -1)
		message(FATAL_ERROR "get_one_icon_component() failed to get one component from '${_instring}'")
	endif(_pos EQUAL -1)

	math(EXPR _posinc "${_pos}+1")

	string(SUBSTRING "${_instring}" 0 ${_pos} ${_outvalue})
	string(SUBSTRING "${_instring}" ${_posinc} -1 ${_outrest})
endmacro(get_one_icon_component)

macro(split_icon_components _infilename _outtheme _outcontext _outsize _outiconfile)
	set(_rest "${_infilename}")

	get_one_icon_component("${_rest}" ${_outtheme} _rest)
	get_one_icon_component("${_rest}" ${_outcontext} _rest)
	get_one_icon_component("${_rest}" ${_outsize} _rest)
	set(${_outiconfile} "${_rest}")
endmacro(split_icon_components)

find_program(GTK_UPDATE_ICON_CACHE gtk-update-icon-cache)
if(NOT GTK_UPDATE_ICON_CACHE)
	message(WARNING "gtk-update-icon-cache not found. Make sure to call ${GTK_UPDATE_ICON_CACHE} -f -t \"${SHARE_INSTALL_PREFIX}/icons/hicolor\" after install and uninstall")
endif(NOT GTK_UPDATE_ICON_CACHE)

set(_update_icon_cache_cmd ${GTK_UPDATE_ICON_CACHE} -f -t "${SHARE_INSTALL_PREFIX}/icons/hicolor")

macro(process_icons _destdir _fileslistvar _install_codevar)
	foreach(srcfile IN LISTS ${_fileslistvar})
		split_icon_components(${srcfile} theme context size iconfile)
		install(FILES ${srcfile}
			DESTINATION ${_destdir}/icons/${theme}/${size}/${context}
			RENAME ${iconfile}
		)
		set(${_install_codevar} "${${_install_codevar}}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}\" \"${_destdir}/icons/${theme}/${size}/${context}/${iconfile}\""
		)
	endforeach(srcfile)
endmacro(process_icons)

macro(add_icon_cache_files _destdir _fileslistvar)
	set(_install_code)

	foreach(_filesvar ${_fileslistvar} ${ARGN})
		process_icons("${_destdir}" ${_filesvar} _install_code)
	endforeach(_filesvar)

	if(GTK_UPDATE_ICON_CACHE)
		install(CODE
			"if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
				execute_process(${_install_code}
					COMMAND ${CMAKE_COMMAND} -E chdir . ${_update_icon_cache_cmd}
				)
			endif(\"\$ENV{DESTDIR}\" STREQUAL \"\")")
	endif(GTK_UPDATE_ICON_CACHE)
endmacro(add_icon_cache_files)

if(GTK_UPDATE_ICON_CACHE)
	add_custom_command(TARGET uninstall POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E chdir . ${_update_icon_cache_cmd}
		COMMENT "Updating icon cache in '${SHARE_INSTALL_PREFIX}/icons/hicolor'"
	)
endif(GTK_UPDATE_ICON_CACHE)

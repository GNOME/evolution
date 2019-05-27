# FindIntltool.cmake
#
# Searches for intltool and gettext. It aborts, if anything cannot be found.
# Requires GETTEXT_PO_DIR to be set to full path of the po/ directory.
#
# Output is:
#   INTLTOOL_UPDATE  - an intltool-update executable path, as found
#   INTLTOOL_EXTRACT  - an intltool-extract executable path, as found
#   INTLTOOL_MERGE  - an intltool-merge executable path, as found
#
# and anything from the FindGettext module.
#
# The below provided macros require GETTEXT_PACKAGE to be set.
#
# intltool_add_check_potfiles_target()
#    Adds a check-potfiles target, which verifies that all files with translations
#    are added in the POTFILES.in file inside GETTEXT_PO_DIR. This macro can be called
#    only inside GETTEXT_PO_DIR.
#
# intltool_add_pot_file_target()
#    Creates a new target pot-file, which generates ${GETTEXT_PACKAGE}.pot file into
#    the CMAKE_CURERNT_BINARY_DIR. This target is not part of ALL.
#    This can be called only inside GETTEXT_PO_DIR.
#
# intltool_process_po_files()
#    Processes all files in the GETTEXT_PO_DIR and generates .gmo files for them
#    in CMAKE_CURRENT_BINARY_DIR. These are added into a new target gmo-files.
#    It also installs them into proper location under LOCALE_INSTALL_DIR.
#    This can be called only inside GETTEXT_PO_DIR.
#
# intltool_setup_po_dir()
#    Shortcut to setup intltool's po/ directory by adding all custom targets
#    and such. this can be called only inside GETTEXT_PO_DIR.
#
# intltool_merge(_in_filename _out_filename ...args)
#    Adds rule to call intltool-merge. The args are optional arguments.
#    This can be called in any folder, only the GETTEXT_PO_DIR should
#    be properly set, otherwise the call will fail.
#
# add_appdata_file(_infilename _outfilename)
#    A shortcut to call intltool-merge() for an appdata file and install it
#    to ${SHARE_INSTALL_PREFIX}/metainfo

include(FindGettext)

if(NOT GETTEXT_FOUND)
	message(FATAL_ERROR "gettext not found, please install at least 0.18.3 version")
endif(NOT GETTEXT_FOUND)

if(GETTEXT_VERSION_STRING VERSION_LESS "0.18.3")
	message(FATAL_ERROR "gettext version 0.18.3+ required, but version '${GETTEXT_VERSION_STRING}' found instead. Please update your gettext")
endif(GETTEXT_VERSION_STRING VERSION_LESS "0.18.3")

find_program(XGETTEXT xgettext)
if(NOT XGETTEXT)
	message(FATAL_ERROR "xgettext executable not found. Please install or update your gettext to at least 0.18.3 version")
endif(NOT XGETTEXT)

find_program(INTLTOOL_UPDATE intltool-update)
if(NOT INTLTOOL_UPDATE)
	message(FATAL_ERROR "intltool-update not found. Please install it (usually part of an 'intltool' package)")
endif(NOT INTLTOOL_UPDATE)

find_program(INTLTOOL_EXTRACT intltool-extract)
if(NOT INTLTOOL_EXTRACT)
	message(FATAL_ERROR "intltool-extract not found. Please install it (usually part of an 'intltool' package)")
endif(NOT INTLTOOL_EXTRACT)

find_program(INTLTOOL_MERGE intltool-merge)
if(NOT INTLTOOL_MERGE)
	message(FATAL_ERROR "intltool-merge not found. Please install it (usually part of an 'intltool' package)")
endif(NOT INTLTOOL_MERGE)

macro(intltool_add_check_potfiles_target)
	if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)
		message(FATAL_ERROR "intltool_add_pot_file_target() can be called only inside GETTEXT_PO_DIR ('${GETTEXT_PO_DIR}'), but it is called inside '${CMAKE_CURRENT_SOURCE_DIR}' instead")
	endif(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)

	add_custom_target(check-potfiles
		COMMAND ${INTLTOOL_UPDATE} -m
		WORKING_DIRECTORY ${GETTEXT_PO_DIR}
	)
endmacro(intltool_add_check_potfiles_target)

macro(intltool_add_pot_file_target)
	if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)
		message(FATAL_ERROR "intltool_add_pot_file_target() can be called only inside GETTEXT_PO_DIR ('${GETTEXT_PO_DIR}'), but it is called inside '${CMAKE_CURRENT_SOURCE_DIR}' instead")
	endif(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)

	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GETTEXT_PACKAGE}.pot
		COMMAND ${CMAKE_COMMAND} -E env INTLTOOL_EXTRACT="${INTLTOOL_EXTRACT}" XGETTEXT="${XGETTEXT}" srcdir=${CMAKE_CURRENT_SOURCE_DIR} ${INTLTOOL_UPDATE} --gettext-package ${GETTEXT_PACKAGE} --pot
	)

	add_custom_target(pot-file
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${GETTEXT_PACKAGE}.pot
	)
endmacro(intltool_add_pot_file_target)

macro(intltool_process_po_files)
	if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)
		message(FATAL_ERROR "intltool_process_po_files() can be called only inside GETTEXT_PO_DIR ('${GETTEXT_PO_DIR}'), but it is called inside '${CMAKE_CURRENT_SOURCE_DIR}' instead")
	endif(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)

	file(GLOB po_files ${GETTEXT_PO_DIR}/*.po)

	set(LINGUAS)
	set(LINGUAS_GMO)

	foreach(file IN LISTS po_files)
		get_filename_component(lang ${file} NAME_WE)
		list(APPEND LINGUAS ${lang})
		list(APPEND LINGUAS_GMO ${lang}.gmo)

		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo
			COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo ${CMAKE_CURRENT_SOURCE_DIR}/${lang}.po
			DEPENDS ${lang}.po
		)

		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo
			DESTINATION ${LOCALE_INSTALL_DIR}/${lang}/LC_MESSAGES/
			RENAME ${GETTEXT_PACKAGE}.mo
		)
		if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo.m)
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo.m
				DESTINATION ${LOCALE_INSTALL_DIR}/${lang}/LC_MESSAGES/
				RENAME ${GETTEXT_PACKAGE}.mo.m
			)
		endif(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo.m)
	endforeach(file)

	add_custom_target(gmo-files ALL
		DEPENDS ${LINGUAS_GMO}
	)
endmacro(intltool_process_po_files)

macro(intltool_setup_po_dir)
	if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)
		message(FATAL_ERROR "intltool_setup_po_dir() can be called only inside GETTEXT_PO_DIR ('${GETTEXT_PO_DIR}'), but it is called inside '${CMAKE_CURRENT_SOURCE_DIR}' instead")
	endif(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL GETTEXT_PO_DIR)

	intltool_add_check_potfiles_target()
	intltool_add_pot_file_target()
	intltool_process_po_files()
endmacro(intltool_setup_po_dir)

macro(intltool_merge _in_filename _out_filename)
	set(_in ${_in_filename})
	set(_out ${_out_filename})

	get_filename_component(_path ${_in} DIRECTORY)
	if(_path STREQUAL "")
		set(_in ${CMAKE_CURRENT_SOURCE_DIR}/${_in})
	endif(_path STREQUAL "")

	get_filename_component(_path ${_out} DIRECTORY)
	if(_path STREQUAL "")
		set(_out ${CMAKE_CURRENT_BINARY_DIR}/${_out})
	endif(_path STREQUAL "")

	set(_has_no_translations OFF)
	set(_args)
	foreach(_arg ${ARGN})
		list(APPEND _args "${_arg}")
		if(_arg STREQUAL "--no-translations")
			set(_has_no_translations ON)
		endif(_arg STREQUAL "--no-translations")
	endforeach(_arg)

	if(_has_no_translations)
		add_custom_command(OUTPUT ${_out}
			COMMAND ${INTLTOOL_MERGE} ${_args} --quiet "${_in}" "${_out}"
			DEPENDS ${_in}
		)
	else(_has_no_translations)
		if(NOT TARGET intltool-merge-cache)
			add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/po/.intltool-merge-cache
				COMMAND ${INTLTOOL_MERGE} ${_args} --quiet --cache="${CMAKE_BINARY_DIR}/po/.intltool-merge-cache" "${GETTEXT_PO_DIR}" "${_in}" "${_out}"
				DEPENDS ${_in}
			)
			add_custom_target(intltool-merge-cache ALL
				DEPENDS ${CMAKE_BINARY_DIR}/po/.intltool-merge-cache)
		endif(NOT TARGET intltool-merge-cache)

		add_custom_command(OUTPUT ${_out}
			COMMAND ${INTLTOOL_MERGE} ${_args} --quiet --cache="${CMAKE_BINARY_DIR}/po/.intltool-merge-cache" "${GETTEXT_PO_DIR}" "${_in}" "${_out}"
			DEPENDS ${_in} intltool-merge-cache
		)
	endif(_has_no_translations)
endmacro(intltool_merge)

macro(add_appdata_file _infilename _outfilename)
	if(NOT TARGET appdata-files)
		add_custom_target(appdata-files ALL)
	endif(NOT TARGET appdata-files)

	set(_out ${_outfilename})
	get_filename_component(_outtarget ${_out} NAME)
	get_filename_component(_path ${_out} DIRECTORY)
	if(_path STREQUAL "")
		set(_out ${CMAKE_CURRENT_BINARY_DIR}/${_out})
	endif(_path STREQUAL "")

	intltool_merge(${_infilename} ${_out} --xml-style --utf8)

	add_custom_target(appdata-${_outtarget}
		DEPENDS ${_out}
	)

	add_dependencies(appdata-files appdata-${_outtarget})

	install(FILES ${_out}
		DESTINATION ${SHARE_INSTALL_PREFIX}/metainfo
	)
endmacro(add_appdata_file)

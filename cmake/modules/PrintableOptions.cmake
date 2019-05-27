# PrintableOptions.cmake
#
# Provides functions to manage printable options,
# which can be printed at the end of the configuration
#
# add_printable_variable_bare(_name)
#    adds variable named _name to the list of prinable options
#
# add_printable_option(_name _description _default_value)
#    the same as option() command, only also notes this option for later printing
#
# add_printable_variable(_name _description _default_value)
#    sets a new cached STRING variable and adds it to the list of printable options
#
# add_printable_variable_path(_name _description _default_value)
#    sets a new cached PATH variable and adds it to the list of printable options
#
# print_build_options()
#    prints all the build options previously added with the above functions

macro(add_printable_variable_bare _name)
	if(_name STREQUAL "")
		message(FATAL_ERROR "variable name cannot be empty")
	endif(_name STREQUAL "")
	list(APPEND _printable_options ${_name})
endmacro()

macro(add_printable_option _name _description _default_value)
	if(_name STREQUAL "")
		message(FATAL_ERROR "option name cannot be empty")
	endif(_name STREQUAL "")
	option(${_name} ${_description} ${_default_value})
	add_printable_variable_bare(${_name})
endmacro()

macro(add_printable_variable _name _description _default_value)
	if(_name STREQUAL "")
		message(FATAL_ERROR "variable name cannot be empty")
	endif(_name STREQUAL "")
	set(${_name} ${_default_value} CACHE STRING ${_description})
	add_printable_variable_bare(${_name})
endmacro()

macro(add_printable_variable_path _name _description _default_value)
	if(_name STREQUAL "")
		message(FATAL_ERROR "path variable name cannot be empty")
	endif(_name STREQUAL "")
	set(${_name} ${_default_value} CACHE PATH ${_description})
	add_printable_variable_bare(${_name})
endmacro()

function(print_build_options)
	message(STATUS "Configure options:")

	set(max_len 0)
	foreach(opt IN LISTS _printable_options)
		string(LENGTH "${opt}" len)
		if(max_len LESS len)
			set(max_len ${len})
		endif(max_len LESS len)
	endforeach()
	math(EXPR max_len "${max_len} + 2")

	foreach(opt IN LISTS _printable_options)
		string(LENGTH "${opt}" len)
		set(str "   ${opt} ")
		foreach (IGNORE RANGE ${len} ${max_len})
			set(str "${str}.")
		endforeach ()
		set(str "${str} ${${opt}}")

		message(STATUS ${str})
	endforeach()
endfunction()

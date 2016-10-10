# CheckTarget.cmake
#
# Defines a custom target 'check', which gathers test programs like 'make check'
# This is taken from https://cmake.org/Wiki/CMakeEmulateMakeCheck
#
# What you do is to call command:
# add_check_test(_name)
#   where _name is the name of the test, as defined by add_executable().
#   Note it is a good idea to use EXCLUDE_FROM_ALL within the add_executable().

include(CTest)

# Disable this to not have verbose tests
set(CMAKE_CTEST_COMMAND ${CMAKE_CTEST_COMMAND} -V)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

macro(add_check_test _name)
	add_test(NAME ${_name} COMMAND ${_name})
	add_dependencies(check ${_name})
endmacro(add_check_test)

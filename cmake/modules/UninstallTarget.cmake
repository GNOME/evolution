# UninstallTarget.cmake
#
# Defines a custom target named 'uninstall'

if(NOT TARGET uninstall)
	configure_file(
		"${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
		"${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
		IMMEDIATE @ONLY)

	add_custom_target(uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake)
endif(NOT TARGET uninstall)

# InstalledTests.cmake
#
# Adds option ENABLE_INSTALLED_TESTS and helper macros to manage
# installed test. There are also set variables:
# INSTALLED_TESTS_EXEC_DIR - where to store installed tests and eventually its data
# INSTALLED_TESTS_META_DIR - where to store .test meta files for installed tests
#
# install_test_if_enabled(_test_target _type _environ)
#    Adds rules to install a test whose target is _test_target (the one
#    used for add_executable()), while the target name should match
#    the executable name. The _type and _environ are used for populating
#    the .test meta file.
#
# install_behave_tests_if_enabled(_testsvar _type _environ)
#    Adds rules to install the 'behave' tests as stored in _testsvar
#    in the current source directory.

include(PrintableOptions)

add_printable_option(ENABLE_INSTALLED_TESTS "Enable installed tests" OFF)

set(INSTALLED_TESTS_EXEC_DIR ${privlibexecdir}/installed-tests)
set(INSTALLED_TESTS_META_DIR ${SHARE_INSTALL_PREFIX}/installed-tests/${PROJECT_NAME})

macro(install_test_if_enabled _test_target _type _environ)
	if(ENABLE_INSTALLED_TESTS)
		set(TEST_TYPE ${_type})
		set(TEST_ENVIRONMENT)
		if(NOT ${_environ} STREQUAL "")
			set(TEST_ENVIRONMENT "env ${_environ} ")
		endif(NOT ${_environ} STREQUAL "")

		set(teststring "[Test]
Type=${TEST_TYPE}
Exec=${TEST_ENVIRONMENT}${INSTALLED_TESTS_EXEC_DIR}/${_test_target}
"
)

		file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_test_target}.test "${teststring}")

		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_test_target}.test
			DESTINATION ${INSTALLED_TESTS_META_DIR}
		)

		install(TARGETS ${_test_target}
			DESTINATION ${INSTALLED_TESTS_EXEC_DIR}
		)
	endif(ENABLE_INSTALLED_TESTS)
endmacro(install_test_if_enabled)

macro(install_behave_tests_if_enabled _testsvar _type _environ)
	if(ENABLE_INSTALLED_TESTS)
		set(TEST_TYPE ${_type})
		set(TEST_ENVIRONMENT)
		if(NOT ${_environ} STREQUAL "")
			set(TEST_ENVIRONMENT "env ${_environ} ")
		endif(NOT ${_environ} STREQUAL "")

		file(GLOB BEHAVE_FEATURES ${CMAKE_CURRENT_SOURCE_DIR}/*.feature)
		file(GLOB BEHAVE_STEP_DEFINITIONS ${CMAKE_CURRENT_SOURCE_DIR}/steps/*.py)
		set(BEHAVE_COMMON_FILES
			${CMAKE_CURRENT_SOURCE_DIR}/environment.py
			${CMAKE_CURRENT_SOURCE_DIR}/common_steps.py
		)

		install(FILES ${BEHAVE_FEATURES} ${BEHAVE_COMMON_FILES}
			DESTINATION ${INSTALLED_TESTS_EXEC_DIR}
		)

		install(FILES ${BEHAVE_STEP_DEFINITIONS}
			DESTINATION ${INSTALLED_TESTS_EXEC_DIR}/steps/
		)

		foreach(_test ${${_testsvar}})
			set(teststring "[Test]
Type=${TEST_TYPE}
Exec=${TEST_ENVIRONMENT}behave ${INSTALLED_TESTS_EXEC_DIR} -t ${_test} -k -f html -o ${_test}.html -f plain
"
)

			file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_test}.test "${teststring}")

			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_test}.test
				DESTINATION ${INSTALLED_TESTS_META_DIR}
			)
		endforeach(_test)
	endif(ENABLE_INSTALLED_TESTS)
endmacro(install_behave_tests_if_enabled)

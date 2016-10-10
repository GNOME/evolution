# CodeCoverageGCOV.cmake
#
# Adds options ENABLE_CODE_COVERAGE, which builds the project with
# code coverage support
#
# It sets variables:
# CODE_COVERAGE_DEFINES - to be used with target_compile_definitions() and similar
# CODE_COVERAGE_CFLAGS - to be used with target_compile_options() and similar for C code
# CODE_COVERAGE_CXXFLAGS - to be used with target_compile_options() and similar for C++ code
# CODE_COVERAGE_LDFLAGS - to be used with target_link_libraries() and similar
#
# These variables should be added as the last in the options, because they change compilation

include(CheckLibraryExists)
include(PrintableOptions)

add_printable_option(ENABLE_CODE_COVERAGE "Enable build with GCOV code coverage" OFF)

if(ENABLE_CODE_COVERAGE)
	if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		CHECK_LIBRARY_EXISTS("gcov" "gcov_exit" "" HAVE_GCOV_LIBRARY)
		if(HAVE_GCOV_LIBRARY)
			set(CODE_COVERAGE_CFLAGS "-O0 -g -fprofile-arcs -ftest-coverage")
			set(CODE_COVERAGE_LDFLAGS "-lgcov")

			add_definitions(-DNDEBUG)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CODE_COVERAGE_CFLAGS}")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CODE_COVERAGE_CFLAGS}")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CODE_COVERAGE_LDFLAGS}")
			set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${CODE_COVERAGE_LDFLAGS}")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CODE_COVERAGE_LDFLAGS}")
			set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${CODE_COVERAGE_LDFLAGS}")
		else(HAVE_GCOV_LIBRARY)
			message(FATAL_ERROR "Cannot fing gcov library, use -DENABLE_CODE_COVERAGE=OFF disable it")
		endif(HAVE_GCOV_LIBRARY)

	else("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		message(FATAL_ERROR "Code coverage requires gcc compiler, use -DENABLE_CODE_COVERAGE=OFF disable it")
	endif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
else(ENABLE_CODE_COVERAGE)
	set(CODE_COVERAGE_DEFINES "")
	set(CODE_COVERAGE_CFLAGS "")
	set(CODE_COVERAGE_CXXFLAGS "")
	set(CODE_COVERAGE_LDFLAGS "")
endif(ENABLE_CODE_COVERAGE)

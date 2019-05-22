# SetupBuildFlags.cmake
#
# Setups compiler/linker flags, skipping those which are not supported.

include(CheckCCompilerFlag)

if(CMAKE_CXX_COMPILER_ID)
	include(CheckCXXCompilerFlag)
endif(CMAKE_CXX_COMPILER_ID)

macro(setup_build_flags _maintainer_mode)
	list(APPEND proposed_flags
		-Werror-implicit-function-declaration
		-Wformat
		-Wformat-security
		-Winit-self
		-Wmissing-declarations
		-Wmissing-noreturn
		-Wpointer-arith
		-Wredundant-decls
		-Wundef
		-Wwrite-strings
		-Wno-cast-function-type
		-no-undefined
		-fno-strict-aliasing
	)

	if(${_maintainer_mode})
		list(APPEND proposed_flags
			-Wall
			-Wextra
			-Wdeprecated-declarations
			-Wmissing-include-dirs
		)
	else(${_maintainer_mode})
		list(APPEND proposed_flags
			-Wno-deprecated-declarations
			-Wno-missing-include-dir)
	endif(${_maintainer_mode})

	list(APPEND proposed_c_flags
		${proposed_flags}
		-Wdeclaration-after-statement
		-Wno-missing-field-initializers
		-Wno-sign-compare
		-Wno-unused-parameter
		-Wnested-externs
	)

	if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
		list(APPEND proposed_c_flags
			-Wno-parentheses-equality
			-Wno-format-nonliteral
		)
	endif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

	list(APPEND proposed_cxx_flags
		${proposed_flags}
		-Wnoexcept
	)

	foreach(flag IN LISTS proposed_c_flags)
		check_c_compiler_flag(${flag} c_flag_${flag}_supported)
		if(c_flag_${flag}_supported)
			set(CMAKE_C_FLAGS "${flag} ${CMAKE_C_FLAGS}")
		endif(c_flag_${flag}_supported)
		unset(c_flag_${flag}_supported)
	endforeach()

	if(CMAKE_CXX_COMPILER_ID)
		foreach(flag IN LISTS proposed_cxx_flags)
			check_cxx_compiler_flag(${flag} cxx_flag_${flag}_supported)
			if(cxx_flag_${flag}_supported)
				set(CMAKE_CXX_FLAGS "${flag} ${CMAKE_CXX_FLAGS}")
			endif(cxx_flag_${flag}_supported)
			unset(cxx_flag_${flag}_supported)
		endforeach()
	endif(CMAKE_CXX_COMPILER_ID)

	if(("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") AND (NOT ${CMAKE_SYSTEM_NAME} MATCHES "BSD"))
		set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-undefined ${CMAKE_EXE_LINKER_FLAGS}")
		set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--no-undefined ${CMAKE_MODULE_LINKER_FLAGS}")
		set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined ${CMAKE_SHARED_LINKER_FLAGS}")
	endif(("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") AND (NOT ${CMAKE_SYSTEM_NAME} MATCHES "BSD"))
endmacro()

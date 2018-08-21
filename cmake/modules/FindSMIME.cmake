# FindSMIME.cmake
#
# Searches for Mozilla's NSS and NSPR libraries, unless -DENABLE_SMIME=OFF is used
#
# The output is:
#    mozilla_nspr - if non-empty, then a pkg-config package name for nspr
#    mozilla_nss - if non-empty, then a pkg-config package name for nss
#    MANUAL_NSPR_INCLUDES - if non-empty, then contains manual nspr include directory, used for target_include_directories() and similar commands
#    MANUAL_NSPR_LIBS - if non-empty, then contains manual nspr libraries, used for target_link_libraries() and similar commands
#    MANUAL_NSS_INCLUDES - if non-empty, then contains manual nss include directory, used for target_include_directories() and similar commands
#    MANUAL_NSS_LIBS - if non-empty, then contains manual nss libraries, used for target_link_libraries() and similar commands
#    MOZILLA_NSS_LIB_DIR - a lib directory where Mozilla stores its libraries

include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(PrintableOptions)
include(PkgConfigEx)

add_printable_option(ENABLE_SMIME "Enable SMIME support through Mozilla nss" ON)
add_printable_variable_path(WITH_NSPR_INCLUDES "Prefix of Mozilla nspr4 includes" "")
add_printable_variable_path(WITH_NSPR_LIBS "Prefix of Mozilla nspr4 libs" "")
add_printable_variable_path(WITH_NSS_INCLUDES "Prefix of Mozilla nss3 includes" "")
add_printable_variable_path(WITH_NSS_LIBS "Prefix of Mozilla nss3 libs" "")

if(NOT ENABLE_SMIME)
	return()
endif(NOT ENABLE_SMIME)

set(mozilla_nspr "")
set(mozilla_nss "")
set(MOZILLA_NSS_LIB_DIR "")

# Use pkg-config when none is specified
if((WITH_NSPR_INCLUDES STREQUAL "") AND (WITH_NSPR_LIBS STREQUAL "") AND (WITH_NSS_INCLUDES STREQUAL "") AND (WITH_NSS_LIBS STREQUAL ""))
	foreach(pkg nspr mozilla-nspr firefox-nspr xulrunner-nspr seamonkey-nspr)
		pkg_check_exists(_have_pkg ${pkg})
		if(_have_pkg)
			set(mozilla_nspr ${pkg})
			break()
		endif(_have_pkg)
	endforeach(pkg)

	foreach(pkg nss mozilla-nss firefox-nss xulrunner-nss seamonkey-nss)
		pkg_check_exists(_have_pkg ${pkg})
		if(_have_pkg)
			set(mozilla_nss ${pkg})
			break()
		endif(_have_pkg)
	endforeach(pkg)

	if((NOT (mozilla_nspr STREQUAL "")) AND (NOT (mozilla_nss STREQUAL "")))
		pkg_check_variable(_nss_libdir ${mozilla_nss} libdir)

		set(MANUAL_NSPR_INCLUDES "")
		set(MANUAL_NSPR_LIBS "")
		set(MANUAL_NSS_INCLUDES "")
		set(MANUAL_NSS_LIBS "")
		set(MOZILLA_NSS_LIB_DIR "${_nss_libdir}")
		return()
	endif((NOT (mozilla_nspr STREQUAL "")) AND (NOT (mozilla_nss STREQUAL "")))
endif()

# Manual search, even when pkg-config failed

# ******************
# Check for NSPR 4
# ******************

if(NOT (WITH_NSPR_INCLUDES STREQUAL ""))
	set(CMAKE_REQUIRED_INCLUDES ${WITH_NSPR_INCLUDES})
endif(NOT (WITH_NSPR_INCLUDES STREQUAL ""))

unset(_have_headers CACHE)

CHECK_INCLUDE_FILES("nspr.h;prio.h" _have_headers)

unset(CMAKE_REQUIRED_INCLUDES)

if(NOT _have_headers)
	message(FATAL_ERROR "NSPR headers not found. Use -DWITH_NSPR_INCLUDES=/path/to/nspr to specify the include dir of NSPR.")
endif(NOT _have_headers)

set(MANUAL_NSPR_INCLUDES "${WITH_NSPR_INCLUDES}")
string(STRIP ${MANUAL_NSPR_INCLUDES} MANUAL_NSPR_INCLUDES)

set(nsprlibs "-lplc4 -lplds4 -lnspr4")

set(CMAKE_REQUIRED_INCLUDES ${MANUAL_NSPR_INCLUDES})
set(CMAKE_REQUIRED_LIBRARIES ${nsprlibs})
unset(_nsprlibs_okay CACHE)
CHECK_C_SOURCE_COMPILES("#include <prinit.h>
			int main(void) { PR_Initialized(); return 0; }" _nsprlibs_okay)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

if(NOT _nsprlibs_okay)
	message(FATAL_ERROR "NSPR libs not found. Use -DWITH_NSPR_LIBS=/path/to/libs to specify the libdir of NSPR")
endif(NOT _nsprlibs_okay)

set(MANUAL_NSPR_LIBS "")

if(NOT (WITH_NSPR_LIBS STREQUAL ""))
	set(MANUAL_NSPR_LIBS "-L${WITH_NSPR_LIBS}")
endif(NOT (WITH_NSPR_LIBS STREQUAL ""))

set(MANUAL_NSPR_LIBS "${MANUAL_NSPR_LIBS} ${nsprlibs}")
string(STRIP ${MANUAL_NSPR_LIBS} MANUAL_NSPR_LIBS)

# *****************
# Check for NSS 3
# *****************

if(NOT (WITH_NSS_INCLUDES STREQUAL ""))
	set(CMAKE_REQUIRED_INCLUDES ${WITH_NSS_INCLUDES})
endif(NOT (WITH_NSS_INCLUDES STREQUAL ""))

if(NOT (WITH_NSPR_INCLUDES STREQUAL ""))
	list(APPEND CMAKE_REQUIRED_INCLUDES ${WITH_NSPR_INCLUDES})
endif(NOT (WITH_NSPR_INCLUDES STREQUAL ""))

unset(_have_headers CACHE)

CHECK_INCLUDE_FILES("nss.h;ssl.h;smime.h" _have_headers)

unset(CMAKE_REQUIRED_INCLUDES)

if(NOT _have_headers)
	message(FATAL_ERROR "NSS headers not found. Use -DWITH_NSS_INCLUDES=/path/to/nss to specify the include dir of NSS.")
endif(NOT _have_headers)

set(MANUAL_NSS_INCLUDES "${WITH_NSS_INCLUDES}")
string(STRIP ${MANUAL_NSS_INCLUDES} MANUAL_NSS_INCLUDES)

set(nsslibs "-lssl3 -lsmime3 -lnss3")

set(CMAKE_REQUIRED_INCLUDES ${MANUAL_NSS_INCLUDES} ${MANUAL_NSPR_INCLUDES})
set(CMAKE_REQUIRED_LIBRARIES ${nsslibs} ${nsprlibs})
unset(_nsslibs_okay CACHE)
CHECK_C_SOURCE_COMPILES("#include <nss.h>
			int main(void) { NSS_Init(\"\"); return 0; }" _nsslibs_okay)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

if(NOT _nsslibs_okay)
	message(FATAL_ERROR "NSS libs not found. Use -DWITH_NSS_LIBS=/path/to/libs to specify the libdir of NSS")
endif(NOT _nsslibs_okay)

set(MANUAL_NSS_LIBS "")

if(NOT (WITH_NSS_LIBS STREQUAL ""))
	set(MANUAL_NSS_LIBS "-L${WITH_NSS_LIBS}")
	set(MOZILLA_NSS_LIB_DIR "${WITH_NSS_LIBS}")
endif(NOT (WITH_NSS_LIBS STREQUAL ""))

set(MANUAL_NSS_LIBS "${MANUAL_NSS_LIBS} ${nsslibs} ${MANUAL_NSPR_LIBS}")
string(STRIP ${MANUAL_NSS_LIBS} MANUAL_NSS_LIBS)

if(MOZILLA_NSS_LIB_DIR STREQUAL "")
	set(MOZILLA_NSS_LIB_DIR "${LIB_INSTALL_DIR}")
endif(MOZILLA_NSS_LIB_DIR STREQUAL "")

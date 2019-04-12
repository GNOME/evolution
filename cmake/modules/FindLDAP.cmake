# FindLDAP.cmake
#
# Searches for OpenLDAP/SunLDAP library
#
# Adds these options:
#    -DWITH_OPENLDAP=ON/OFF/PATH - enable/disable OpenLDAP, eventually set prefix to find it
#    -DWITH_SUNLDAP=OFF/ON/PATH - enable/disable SunLDAP, eventually set prefix to find it
#    -DWITH_STATIC_LDAP=OFF/ON - enable/disable static LDAP linking
#
# The OpenLDAP has precedence over SunLDAP, if both are specified. The default is to use OpenLDAP.
#
# The output is:
#    HAVE_LDAP - set to ON, if LDAP support is enabled and libraries found
#    SUNLDAP - set to ON, when using SunLDAP implementation
#    LDAP_CFLAGS - CFLAGS to use with target_compile_options() and similar commands
#    LDAP_INCLUDE_DIRS - include directories to use with target_include_directories() and similar commands
#    LDAP_LIBS - libraries to use with target_link_libraries() and similar commands

include(CheckCSourceCompiles)
include(CheckLibraryExists)
include(PrintableOptions)

add_printable_variable_path(WITH_OPENLDAP "Enable LDAP support using OpenLDAP, default ON" "ON")
add_printable_variable_path(WITH_SUNLDAP "Enable LDAP support using SunLDAP, default OFF" "OFF")
add_printable_option(WITH_STATIC_LDAP "Link LDAP statically, default OFF" OFF)

if((NOT WITH_OPENLDAP) AND (NOT WITH_SUNLDAP))
	return()
endif((NOT WITH_OPENLDAP) AND (NOT WITH_SUNLDAP))

string(LENGTH "${CMAKE_BINARY_DIR}" bindirlen)
string(LENGTH "${WITH_OPENLDAP}" maxlen)
if(maxlen LESS bindirlen)
	set(substr "***")
else(maxlen LESS bindirlen)
	string(SUBSTRING "${WITH_OPENLDAP}" 0 ${bindirlen} substr)
endif(maxlen LESS bindirlen)
string(TOUPPER "${WITH_OPENLDAP}" optupper)

if(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))
	set(WITH_OPENLDAP "/usr")
endif(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))

string(LENGTH "${WITH_SUNLDAP}" maxlen)
if(maxlen LESS bindirlen)
	set(substr "***")
else(maxlen LESS bindirlen)
	string(SUBSTRING "${WITH_SUNLDAP}" 0 ${bindirlen} substr)
endif(maxlen LESS bindirlen)
string(TOUPPER "${WITH_SUNLDAP}" optupper)

if(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))
	set(WITH_SUNLDAP "/usr")
endif(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))

unset(bindirlen)
unset(maxlen)
unset(substr)
unset(optupper)

set(HAVE_LDAP ON)
set(SUNLDAP OFF)

macro(add_ldap_lib_if_provides _lib _symbol)
	CHECK_LIBRARY_EXISTS(${_lib} ${_symbol} "" lib${_lib}_provides_${_symbol})
	if(lib${_lib}_provides_${_symbol})
		set(LDAP_LIBS "${LDAP_LIBS} -l${_lib}")
	endif(lib${_lib}_provides_${_symbol})
endmacro(add_ldap_lib_if_provides)

set(LDAP_PREFIX "")
if(WITH_OPENLDAP)
	set(LDAP_PREFIX "${WITH_OPENLDAP}")
else(WITH_OPENLDAP)
	set(LDAP_PREFIX "${WITH_SUNLDAP}")
	set(SUNLDAP ON)
endif(WITH_OPENLDAP)

set(LDAP_CFLAGS "")
set(LDAP_INCLUDE_DIRS "${LDAP_PREFIX}/include")
set(LDAP_LIBS "-L${LDAP_PREFIX}/lib${LIB_SUFFIX}")

set(CMAKE_REQUIRED_INCLUDES "${LDAP_INCLUDE_DIRS}")
set(CMAKE_REQUIRED_LIBRARIES "${LDAP_LIBS}")

if(WITH_OPENLDAP)
	CHECK_C_SOURCE_COMPILES("#include \"ldap.h\"
				int main(void) {
					/* LDAP_VENDOR_VERSION is 0 if OpenLDAP is built from git/master */
					#if !defined(LDAP_VENDOR_VERSION) || (LDAP_VENDOR_VERSION != 0 && LDAP_VENDOR_VERSION < 20000)
					#error OpenLDAP version not at least 2.0
					#endif
					return 0; }" openldap_2_x)
	if(NOT openldap_2_x)
		message(FATAL_ERROR "At least 2.0 OpenLDAP version required; either use -DWITH_OPENLDAP=OFF argument to cmake command to disable LDAP support, or install OpenLDAP")
	endif(NOT openldap_2_x)
else(WITH_OPENLDAP)
	CHECK_C_SOURCE_COMPILES("#include \"ldap.h\"
				int main(void) {
					#if !defined(LDAP_VENDOR_VERSION) || LDAP_VENDOR_VERSION < 500
					#error SunLDAP version not at least 2.0
					#endif
					return 0; }" sunldap_2_x)
	if(NOT sunldap_2_x)
		message(FATAL_ERROR "At least 2.0 SunLDAP version required; either use -DWITH_SUNLDAP=OFF argument to cmake command to disable LDAP support, or install SunLDAP")
	endif(NOT sunldap_2_x)
endif(WITH_OPENLDAP)

add_ldap_lib_if_provides(resolv res_query)
add_ldap_lib_if_provides(resolv __res_query)
add_ldap_lib_if_provides(socket bind)
CHECK_LIBRARY_EXISTS(lber ber_get_tag "" liblber_provides_ber_get_tag)
if(liblber_provides_ber_get_tag)
	if(WITH_STATIC_LDAP)
		set(LDAP_LIBS "${LDAP_LIBS} ${LDAP_PREFIX}/lib${LIB_SUFFIX}/liblber.a")
#		# libldap might depend on OpenSSL... We need to pull
#		# in the dependency libs explicitly here since we're
#		# not using libtool for the configure test.
#		if test -f ${LDAP_PREFIX}/lib${LIB_SUFFIX}/libldap.la; then
#			LDAP_LIBS="`. ${LDAP_PREFIX}/libPLIB_SUFFIX}/libldap.la; echo $dependency_libs` $LDAP_LIBS"
#		fi
	else(WITH_STATIC_LDAP)
		set(LDAP_LIBS "${LDAP_LIBS} -llber")
	endif(WITH_STATIC_LDAP)
endif(liblber_provides_ber_get_tag)

CHECK_LIBRARY_EXISTS(ldap ldap_open "" libldap_provides_ldap_open)
if(libldap_provides_ldap_open)
	if(WITH_STATIC_LDAP)
		set(LDAP_LIBS "${LDAP_LIBS} ${LDAP_PREFIX}/lib${LIB_SUFFIX}/libldap.a")
	else(WITH_STATIC_LDAP)
		set(LDAP_LIBS "${LDAP_LIBS} -lldap")
	endif(WITH_STATIC_LDAP)
else(libldap_provides_ldap_open)
	if(WITH_OPENLDAP)
		message(FATAL_ERROR "Could not find OpenLDAP libraries; either use -DWITH_OPENLDAP=OFF argument to cmake command to disable LDAP support, or install OpenLDAP")
	else(WITH_OPENLDAP)
		message(FATAL_ERROR "Could not find SunLDAP libraries; either use -DWITH_SUNLDAP=OFF argument to cmake command to disable LDAP support, or install SunLDAP")
	endif(WITH_OPENLDAP)
endif(libldap_provides_ldap_open)

unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

add_definitions(-DLDAP_DEPRECATED)

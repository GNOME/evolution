# GLibTools.cmake
#
# Provides functions to run glib tools.
#
# Functions:
#
# glib_mkenums(_output_filename_noext _enums_header _define_name)
#    runs glib-mkenums to generate enumtypes .h and .c files from _enums_header.
#    It searches for files in the current source directory and exports to the current
#    binary directory.
#
#    An example call is:
#        glib_mkenums(camel-enumtypes camel-enums.h CAMEL_ENUMTYPES_H)
#        which uses camel-enums.h as the source of known enums and generates
#        camel-enumtypes.h which will use the CAMEL_ENUMTYPES_H define
#        and also generates camel-enumtypes.c with the needed code.
#
# glib_genmarshal(_output_filename_noext _prefix _marshallist_filename)
#    runs glib-genmarshal to process ${_marshallist_filename} to ${_output_filename_noext}.c
#    and ${_output_filename_noext}.h files in the current binary directory, using
#    the ${_prefix} as the function prefix.
#
# gdbus_codegen(_xml _interface_prefix _c_namespace _files_prefix _list_gens)
#    runs gdbus-codegen to generate GDBus code from _xml file description,
#    using _interface_prefix, _c_namespace and _files_prefix as arguments.
#    The _list_gens is a list variable are stored expected generated files.
#
#    An example call is:
#        set(GENERATED_DBUS_LOCALE
#               e-dbus-localed.c
#	        e-dbus-localed.h
#        )
#        gdbus_codegen(org.freedesktop.locale1.xml org.freedesktop. E_DBus e-dbus-localed GENERATED_DBUS_LOCALE)
#
# gdbus_codegen_custom(_xml _interface_prefix _c_namespace _files_prefix _list_gens _args)
#    The same as gdbus_codegen() except allows to pass other arguments to the call,
#    like for example --c-generate-object-manager
#
# add_gsettings_schemas(_target _schema0 ...)
#    Adds one or more GSettings schemas. The extension is supposed to be .gschema.xml. The schema file generation
#    is added as a dependency of _target.
#
# glib_compile_resources _sourcedir _outputprefix _cname _inxml ...deps)
#    Calls glib-compile-resources as defined in _inxml and using _outputprefix and_cname as other arguments
#    beside _sourcedir. The optional arguments are other dependencies.

include(PkgConfigEx)
include(UninstallTarget)

find_program(GLIB_MKENUMS glib-mkenums)
if(NOT GLIB_MKENUMS)
	message(FATAL_ERROR "Cannot find glib-mkenums, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_MKENUMS)

function(glib_mkenums _output_filename_noext _enums_header _define_name)
	set(HEADER_TMPL "
/*** BEGIN file-header ***/
#ifndef ${_define_name}
#define ${_define_name}
/*** END file-header ***/

/*** BEGIN file-production ***/

#include <glib-object.h>

G_BEGIN_DECLS

/* Enumerations from \"@filename@\" */

/*** END file-production ***/

/*** BEGIN enumeration-production ***/
#define @ENUMPREFIX@_TYPE_@ENUMSHORT@	(@enum_name@_get_type())
GType @enum_name@_get_type	(void) G_GNUC_CONST;

/*** END enumeration-production ***/

/*** BEGIN file-tail ***/
G_END_DECLS

#endif /* ${_define_name} */
/*** END file-tail ***/")

	file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/enumtypes-${_output_filename_noext}.h.tmpl" "${HEADER_TMPL}\n")

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
		COMMAND ${GLIB_MKENUMS} --template "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/enumtypes-${_output_filename_noext}.h.tmpl" "${CMAKE_CURRENT_SOURCE_DIR}/${_enums_header}" >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_enums_header}
	)

set(SOURCE_TMPL "
/*** BEGIN file-header ***/
#include \"${_output_filename_noext}.h\"
/*** END file-header ***/

/*** BEGIN file-production ***/
/* enumerations from \"@filename@\" */
#include \"@filename@\"

/*** END file-production ***/

/*** BEGIN value-header ***/
GType
@enum_name@_get_type (void)
{
	static volatile gsize the_type__volatile = 0;

	if (g_once_init_enter (&the_type__volatile)) {
		static const G\@Type\@Value values[] = {
/*** END value-header ***/

/*** BEGIN value-production ***/
			{ \@VALUENAME\@,
			  \"@VALUENAME@\",
			  \"@valuenick@\" },
/*** END value-production ***/

/*** BEGIN value-tail ***/
			{ 0, NULL, NULL }
		};
		GType the_type = g_\@type\@_register_static (
			g_intern_static_string (\"@EnumName@\"),
			values);
		g_once_init_leave (&the_type__volatile, the_type);
	}
	return the_type__volatile;
}

/*** END value-tail ***/")

	file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/enumtypes-${_output_filename_noext}.c.tmpl" "${SOURCE_TMPL}\n")

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
		COMMAND ${GLIB_MKENUMS} --template "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/enumtypes-${_output_filename_noext}.c.tmpl" "${CMAKE_CURRENT_SOURCE_DIR}/${_enums_header}" >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_enums_header}
	)
endfunction(glib_mkenums)

find_program(GLIB_GENMARSHAL glib-genmarshal)
if(NOT GLIB_GENMARSHAL)
	message(FATAL_ERROR "Cannot find glib-genmarshal, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_GENMARSHAL)

function(glib_genmarshal _output_filename_noext _prefix _marshallist_filename)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
		COMMAND ${GLIB_GENMARSHAL} --header --prefix=${_prefix} "${CMAKE_CURRENT_SOURCE_DIR}/${_marshallist_filename}" >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h.tmp
		COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.h
		DEPENDS ${_marshallist_filename}
	)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
		COMMAND ${CMAKE_COMMAND} -E echo " #include \\\"${_output_filename_noext}.h\\\"" >${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c.tmp
		COMMAND ${GLIB_GENMARSHAL} --body --prefix=${_prefix} "${CMAKE_CURRENT_SOURCE_DIR}/${_marshallist_filename}" >>${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c.tmp
		COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c.tmp ${CMAKE_CURRENT_BINARY_DIR}/${_output_filename_noext}.c
		DEPENDS ${_marshallist_filename}
	)
endfunction(glib_genmarshal)

find_program(GDBUS_CODEGEN gdbus-codegen)
if(NOT GDBUS_CODEGEN)
	message(FATAL_ERROR "Cannot find gdbus-codegen, which is required to build ${PROJECT_NAME}")
endif(NOT GDBUS_CODEGEN)

function(gdbus_codegen_custom _xml _interface_prefix _c_namespace _files_prefix _list_gens _args)
	add_custom_command(
		OUTPUT ${${_list_gens}}
		COMMAND ${GDBUS_CODEGEN}
		ARGS --interface-prefix ${_interface_prefix}
			--c-namespace ${_c_namespace}
			--generate-c-code ${_files_prefix}
			--generate-docbook ${_files_prefix}
			${_args}
			${CMAKE_CURRENT_SOURCE_DIR}/${_xml}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${_xml}
		VERBATIM
	)
endfunction(gdbus_codegen_custom)

function(gdbus_codegen _xml _interface_prefix _c_namespace _files_prefix _list_gens)
	gdbus_codegen_custom(${_xml} ${_interface_prefix} ${_c_namespace} ${_files_prefix} ${_list_gens} "")
endfunction(gdbus_codegen)

add_printable_option(ENABLE_SCHEMAS_COMPILE "Enable GSettings regeneration of gschemas.compile on install" ON)

if(CMAKE_CROSSCOMPILING)
	find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)
else(CMAKE_CROSSCOMPILING)
	pkg_check_variable(GLIB_COMPILE_SCHEMAS gio-2.0 glib_compile_schemas)
endif(CMAKE_CROSSCOMPILING)

if(NOT GLIB_COMPILE_SCHEMAS)
	message(FATAL_ERROR "Cannot find glib-compile-schemas, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_COMPILE_SCHEMAS)

set(GSETTINGS_SCHEMAS_DIR "${SHARE_INSTALL_PREFIX}/glib-2.0/schemas/")

macro(add_gsettings_schemas _target _schema0)
	set(_install_code)

	foreach(_schema ${_schema0} ${ARGN})
		string(REPLACE ".xml" ".valid" _outputfile "${_schema}")
		get_filename_component(_outputfile "${_outputfile}" NAME)

		get_filename_component(_schema_fullname "${_schema}" DIRECTORY)
		get_filename_component(_schema_filename "${_schema}" NAME)
		if(_schema_fullname STREQUAL "")
			set(_schema_fullname ${CMAKE_CURRENT_SOURCE_DIR}/${_schema})
		else(_schema_fullname STREQUAL "")
			set(_schema_fullname ${_schema})
		endif(_schema_fullname STREQUAL "")

		add_custom_command(
			OUTPUT ${_outputfile}
			COMMAND ${GLIB_COMPILE_SCHEMAS} --strict --dry-run --schema-file=${_schema_fullname}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_schema_fullname}" "${CMAKE_CURRENT_BINARY_DIR}/${_outputfile}"
			DEPENDS ${_schema_fullname}
			VERBATIM
		)
		add_custom_target(gsettings-schemas-${_schema_filename} ALL DEPENDS ${_outputfile})
		add_dependencies(${_target} gsettings-schemas-${_schema_filename})
		if(ENABLE_SCHEMAS_COMPILE)
			# this is required to compile gsettings schemas like after 'make install,
			# because there is no better way in CMake to run a code/script after
			# the whole `make install`
			set(_install_code "${_install_code}
				COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${_schema_fullname}\" \"${GSETTINGS_SCHEMAS_DIR}\""
			)
		endif(ENABLE_SCHEMAS_COMPILE)

		# Do both, to have 'uninstall' working properly
		install(FILES ${_schema_fullname}
			DESTINATION ${GSETTINGS_SCHEMAS_DIR})
	endforeach(_schema)

	if(_install_code)
		# Compile gsettings schemas and ensure that all of them are in the place.
		install(CODE
			"if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
				execute_process(${_install_code}
					COMMAND ${CMAKE_COMMAND} -E chdir . \"${GLIB_COMPILE_SCHEMAS}\" \"${GSETTINGS_SCHEMAS_DIR}\"
				)
			endif(\"\$ENV{DESTDIR}\" STREQUAL \"\")")
	endif(_install_code)
endmacro(add_gsettings_schemas)

# This is called too early, when the schemas are not installed yet during `make install`
#
# compile_gsettings_schemas()
#    Optionally (based on ENABLE_SCHEMAS_COMPILE) recompiles schemas at the destination folder
#    after install. It's necessary to call it as the last command in the toplevel CMakeLists.txt,
#    thus the compile runs when all the schemas are installed.
#
if(ENABLE_SCHEMAS_COMPILE)
	add_custom_command(TARGET uninstall POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E chdir . "${GLIB_COMPILE_SCHEMAS}" "${GSETTINGS_SCHEMAS_DIR}"
		COMMENT "Recompile GSettings schemas in '${GSETTINGS_SCHEMAS_DIR}'"
	)
endif(ENABLE_SCHEMAS_COMPILE)

find_program(GLIB_COMPILE_RESOURCES glib-compile-resources)
if(NOT GLIB_COMPILE_RESOURCES)
	message(FATAL_ERROR "Cannot find glib-compile-resources, which is required to build ${PROJECT_NAME}")
endif(NOT GLIB_COMPILE_RESOURCES)

macro(glib_compile_resources _sourcedir _outputprefix _cname _inxml)
	add_custom_command(
		OUTPUT ${_outputprefix}.h
		COMMAND ${GLIB_COMPILE_RESOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${_inxml} --target=${_outputprefix}.h --sourcedir=${_sourcedir} --c-name ${_cname} --generate-header
		DEPENDS ${_inxml} ${ARGN}
		VERBATIM
	)
	add_custom_command(
		OUTPUT ${_outputprefix}.c
		COMMAND ${GLIB_COMPILE_RESOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${_inxml} --target=${_outputprefix}.c --sourcedir=${_sourcedir} --c-name ${_cname} --generate-source
		DEPENDS ${_inxml} ${ARGN}
		VERBATIM
	)
endmacro(glib_compile_resources)

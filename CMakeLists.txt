# Evolution build script

cmake_minimum_required(VERSION 3.0)
cmake_policy(VERSION 3.0)

project(evolution
	VERSION 3.23.1
	LANGUAGES C CXX)
set(PROJECT_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution")
set(PROJECT_URL "http://wiki.gnome.org/Apps/Evolution/")

# Base Version: This is for API/version tracking for things like
# D-Bus server files. This should always be the major/minor of
# the stable version or stable version to be.
set(BASE_VERSION 3.24)

# Used for pkg-config files
set(INTERFACE_VERSION 3.0)

math(EXPR _is_devel_version "${PROJECT_VERSION_MINOR}%2")
if(_is_devel_version EQUAL 1)
	math(EXPR _minor "${PROJECT_VERSION_MINOR}-1")
	if(_minor EQUAL -1)
		set(_minor 98)
		math(EXPR _major "${PROJECT_VERSION_MAJOR}-1")
	else(_minor EQUAL -1)
		set(_major ${PROJECT_VERSION_MAJOR})
	endif(_minor EQUAL -1)

	set(STABLE_VERSION "${_major}.${_minor}")
else(_is_devel_version EQUAL 1)
	unset(STABLE_VERSION)
endif(_is_devel_version EQUAL 1)

# Required for FindIntltool module
set(GETTEXT_PACKAGE ${PROJECT_NAME}-${BASE_VERSION})
set(GETTEXT_PO_DIR ${CMAKE_SOURCE_DIR}/po)

# Required for 'disttest' and 'ditcheck' of DistTarget module
set(PROJECT_DISTCONFIGURE_PARAMS
	-DENABLE_GTK_DOC=ON
	-DENABLE_CONTACT_MAPS=ON
	-DENABLE_INSTALLED_TESTS=ON
	-DWITH_HELP=ON
	-DWITH_GLADE_CATALOG=ON
)

# Keep these two definitions in agreement.
set(glib_minimum_version 2.46)
set(glib_encoded_version GLIB_VERSION_2_46)

# Keep these two definitions in agreement.
set(gdk_minimum_version 3.10)
set(gdk_encoded_version GDK_VERSION_3_10)

# Keep these two definitions in agreement.
set(soup_minimum_version 2.42)
set(soup_encoded_version SOUP_VERSION_2_42)

# Warn about API usage that violates our minimum requirements.
add_definitions(-DGLIB_VERSION_MAX_ALLOWED=${glib_encoded_version})
add_definitions(-DGDK_VERSION_MAX_ALLOWED=${gdk_encoded_version})
add_definitions(-DSOUP_VERSION_MAX_ALLOWED=${soup_encoded_version})

# These will suppress warnings about newly-deprecated symbols. Ideally
# these settings should match our minimum requirements and we will clean
# up any new deprecation warnings after bumping our minimum requirements.
# But if the warnings get to be overwhelming, use fixed versions instead.
add_definitions(-DGLIB_VERSION_MIN_REQUIRED=${glib_encoded_version})
add_definitions(-DGDK_VERSION_MIN_REQUIRED=${gdk_encoded_version})
add_definitions(-DSOUP_VERSION_MIN_REQUIRED=${soup_encoded_version})

set(eds_minimum_version ${PROJECT_VERSION})
set(enchant_minimum_version 1.1.7)
set(gcr_minimum_version 3.4)
set(gdk_pixbuf_minimum_version 2.24.0)
set(gnome_desktop_minimum_version 2.91.3)
set(gsettings_desktop_schemas_minimum_version 2.91.92)
set(libpst_minimum_version 0.6.54)
set(libxml_minimum_version 2.7.3)
set(shared_mime_info_minimum_version 0.22)
set(webkit2gtk_minimum_version 2.13.90)

# Optional Packages
set(champlain_minimum_version 0.12)
set(clutter_gtk_minimum_version 0.90)
set(geocode_glib_minimum_version 3.10)
set(gladeui_minimum_version 3.10.0)
set(gnome_autoar_minimum_version 0.1.1)
set(gweather_minimum_version 3.10)
set(libcanberra_gtk_minimum_version 0.25)
set(libnotify_minimum_version 0.7)

# Load modules from the source tree
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_BUILD_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
# CMAKE_INSTALL_RPATH is set below

# CMake modules
include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)

# Project custom modules
include(PrintableOptions)

add_printable_variable(LIB_SUFFIX "Library directory suffix, usually defined to '64' for x86_64 systems" "")
add_printable_variable_bare(CMAKE_INSTALL_PREFIX)
add_printable_variable_path(BIN_INSTALL_DIR "Install directory for binary files, defaults to CMAKE_INSTALL_PREFIX/bin" "${CMAKE_INSTALL_PREFIX}/bin")
add_printable_variable_path(INCLUDE_INSTALL_DIR "Install directory for header files, defaults to CMAKE_INSTALL_PREFIX/include" "${CMAKE_INSTALL_PREFIX}/include")
add_printable_variable_path(LIB_INSTALL_DIR "Install directory for library files, defaults to CMAKE_INSTALL_PREFIX/lib{LIB_SUFFIX}" "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
add_printable_variable_path(LIBEXEC_INSTALL_DIR "Install directory for library executable files, defaults to CMAKE_INSTALL_PREFIX/libexec" "${CMAKE_INSTALL_PREFIX}/libexec")
add_printable_variable_path(SHARE_INSTALL_DIR "Install directory for shared files, defaults to CMAKE_INSTALL_PREFIX/share" "${CMAKE_INSTALL_PREFIX}/share")
add_printable_variable_path(LOCALE_INSTALL_DIR "Install directory for locale files, defaults to SHARE_INSTALL_DIR/locale" "${SHARE_INSTALL_DIR}/locale")
add_printable_variable_path(SYSCONF_INSTALL_DIR "Install directory for system configuration files, defaults to CMAKE_INSTALL_PREFIX/etc" "${CMAKE_INSTALL_PREFIX}/etc")

# ******************************
# Special directories
# ******************************

# If you add something here, consider whether or not you also
# need to add it to one or more .pc.in files (for Connector, etc)

set(privdatadir "${SHARE_INSTALL_DIR}/${PROJECT_NAME}")
set(privincludedir "${INCLUDE_INSTALL_DIR}/${PROJECT_NAME}")
set(privlibdir "${LIB_INSTALL_DIR}/${PROJECT_NAME}")
set(privlibexecdir "${LIBEXEC_INSTALL_DIR}/${PROJECT_NAME}")

SET(CMAKE_INSTALL_RPATH "${LIB_INSTALL_DIR}:${privlibdir}")

if(WIN32)
	# On Win32 there is no "rpath" mechanism. We install the private
	# shared libraries in $libdir, meaning the DLLs will actually be in
	# $bindir. This means just having $bindir in PATH will be enough.
	set(privsolibdir "${LIB_INSTALL_DIR}")
else(WIN32)
	set(privsolibdir "${privlibdir}")
endif(WIN32)

set(errordir "${privdatadir}/errors")
set(etspecdir "${privdatadir}/etspec")
set(evolutionhelpdir "${privdatadir}/help")
set(icondir "${privdatadir}/icons")
set(imagesdir "${privdatadir}/images")
set(moduledir "${privlibdir}/modules")
set(plugindir "${privlibdir}/plugins")
set(soundsdir "${privdatadir}/sounds")
set(uidir "${privdatadir}/ui")
set(viewsdir "${privdatadir}/views")
set(webextensionsdir "${privlibdir}/web-extensions")
set(webextensionswebkiteditordir "${webextensionsdir}/webkit-editor")

# ******************************
# Dependencies
# ******************************

include(CodeCoverageGCOV)
include(CheckTarget)
include(DistTargets)
include(EvolutionMacros)
include(GLibTools)
include(GtkDoc)
include(IconCache)
include(InstalledTests)
include(PkgConfigEx)
include(SetupBuildFlags)
include(UninstallTarget)

include(FindIntltool)
include(FindLDAP)
include(FindSMIME)

add_printable_option(ENABLE_MAINTAINER_MODE "Enable maintainer mode" OFF)
add_printable_variable(VERSION_SUBSTRING "Version substring, for packagers" "")
add_printable_variable(VERSION_COMMENT "Define if you want a comment appended to the version number" "")

if(ENABLE_MAINTAINER_MODE)
	set(BUILD_TESTING ON)
endif(ENABLE_MAINTAINER_MODE)

# Setup compiler/linker flags
setup_build_flags(${ENABLE_MAINTAINER_MODE})

set(MATH_LDFLAGS -lm)

CHECK_INCLUDE_FILE(sys/wait.h HAVE_SYS_WAIT_H)
CHECK_INCLUDE_FILE(X11/XF86keysym.h HAVE_XFREE)
CHECK_FUNCTION_EXISTS(mkdtemp HAVE_MKDTEMP)
CHECK_FUNCTION_EXISTS(nl_langinfo HAVE_NL_LANGINFO)

# ******************************
# executables
# ******************************

find_program(KILL_PROCESS_COMMAND killall)
if(NOT KILL_PROCESS_COMMAND)
	find_program(KILL_PROCESS_COMMAND pkill)
endif(NOT KILL_PROCESS_COMMAND)

if(NOT KILL_PROCESS_COMMAND)
	message(WARNING "Could not find a command to kill a process by name")
endif(NOT KILL_PROCESS_COMMAND)

# ******************************
# Check for nl_langinfo features
# ******************************

CHECK_C_SOURCE_COMPILES("#include <langinfo.h>
			int main(void) { char *detail = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT); return 0; }" HAVE__NL_MEASUREMENT_MEASUREMENT)

# ******************************
# Dependencies
# ******************************

if(WIN32)
	set(GIO_UNIX_REQUIREMENT "gio-windows-2.0")
else(WIN32)
	set(GIO_UNIX_REQUIREMENT "gio-unix-2.0")
endif(WIN32)

add_printable_option(ENABLE_GNOME_DESKTOP "Enable GNOME desktop dependency for thumbnails" ON)

if(ENABLE_GNOME_DESKTOP)
	set(GNOME_DESKTOP_DEPENDENCY "gnome-desktop-3.0")
	set(HAVE_GNOME_DESKTOP ON)

	# It's checked for it twice, this one gives a hint to disable the dependency if not found
	pkg_check_modules_for_option(ENABLE_GNOME_DESKTOP "GNOME desktop dependency for thumbnails" GNOME_DESKTOP ${GNOME_DESKTOP_DEPENDENCY}>=${gnome_desktop_minimum_version})
else(ENABLE_GNOME_DESKTOP)
	set(GNOME_DESKTOP_DEPENDENCY "")
	set(HAVE_GNOME_DESKTOP OFF)
endif(ENABLE_GNOME_DESKTOP)

pkg_check_modules(GNOME_PLATFORM REQUIRED
	cairo-gobject
	gail-3.0>=${gdk_minimum_version}
	gcr-3>=${gcr_minimum_version}
	gdk-pixbuf-2.0>=${gdk_pixbuf_minimum_version}
	gio-2.0>=${glib_minimum_version}
	${GIO_UNIX_REQUIREMENT}
	gmodule-2.0>=${glib_minimum_version}
	${GNOME_DESKTOP_DEPENDENCY}
	gsettings-desktop-schemas>=${gsettings_desktop_schemas_minimum_version}
	gtk+-3.0>=${gdk_minimum_version}
	libxml-2.0>=${libxml_minimum_version}
	shared-mime-info>=${shared_mime_info_minimum_version}
	webkit2gtk-4.0>=${webkit2gtk_minimum_version}
)

pkg_check_modules(EVOLUTION_DATA_SERVER REQUIRED
	camel-1.2>=${eds_minimum_version}
	libebook-1.2>=${eds_minimum_version}
	libecal-1.2>=${eds_minimum_version}
	libedataserver-1.2>=${eds_minimum_version}
	libedataserverui-1.2>=${eds_minimum_version}
	libebackend-1.2>=${eds_minimum_version}
)

pkg_check_modules(A11Y REQUIRED atk)
pkg_check_modules(ENCHANT REQUIRED enchant>=${enchant_minimum_version})
pkg_check_modules(LIBSOUP REQUIRED libsoup-2.4>=${soup_minimum_version})
pkg_check_modules(WEB_EXTENSION REQUIRED webkit2gtk-4.0>=${webkit2gtk_minimum_version})

# ******************************
# Canberra / Canberra-GTK Sound
# ******************************

add_printable_option(ENABLE_CANBERRA "Enable Canberra and Canberra-GTK sound" ON)

if(ENABLE_CANBERRA)
	pkg_check_modules_for_option(ENABLE_CANBERRA "Canberra and Canberra-GTK sound" CANBERRA libcanberra-gtk3>=${libcanberra_gtk_minimum_version})
	set(HAVE_CANBERRA ON)
endif(ENABLE_CANBERRA)

# ******************************
# Archives Integration / gnome-autoar
# ******************************

add_printable_option(ENABLE_AUTOAR "Enable archives support in attachments" ON)

if(ENABLE_AUTOAR)
	pkg_check_modules_for_option(ENABLE_AUTOAR "archives support in attachments" AUTOAR
		gnome-autoar-0>=${gnome_autoar_minimum_version}
		gnome-autoar-gtk-0>=${gnome_autoar_minimum_version}
	)
	set(HAVE_AUTOAR ON)
endif(ENABLE_AUTOAR)

# ******************************
# User Documentation
# ******************************

add_printable_option(WITH_HELP "Build user documentation" ON)

if(WITH_HELP)
	find_program(ITSTOOL itstool)
	if(NOT ITSTOOL)
		message(FATAL_ERROR "Cannot find itstool, either install it or disable help build by adding -DWITH_HELP=OFF argument to cmake command")
	endif(NOT ITSTOOL)
endif(WITH_HELP)

# ******************************
# iconv checking
# ******************************

set(CMAKE_REQUIRED_LIBRARIES "-liconv")
CHECK_C_SOURCE_COMPILES("#include <iconv.h>
			#include <stdlib.h>
			int main(void) { iconv_t cd; cd = iconv_open (\"UTF-8\", \"ISO-8859-1\"); return 0; }" HAVE_LIBICONV)
unset(CMAKE_REQUIRED_LIBRARIES)

if(HAVE_LIBICONV)
	set(ICONV_LIBS "-liconv")
	set(HAVE_ICONV ON)
else(HAVE_LIBICONV)
	set(ICONV_LIBS "")
	CHECK_FUNCTION_EXISTS(iconv HAVE_ICONV)
endif(HAVE_LIBICONV)

if(NOT HAVE_ICONV)
	message(FATAL_ERROR "You need to install a working iconv implementation, such as ftp://ftp.gnu.org/pub/gnu/libiconv")
endif(NOT HAVE_ICONV)

set(CMAKE_REQUIRED_LIBRARIES ${ICONV_LIBS})
CHECK_C_SOURCE_RUNS("#include \"${CMAKE_SOURCE_DIR}/iconv-detect.c\"" _correct_iconv)
unset(CMAKE_REQUIRED_LIBRARIES)

if(NOT _correct_iconv)
	message(FATAL_ERROR "You need to install a working iconv implementation, such as ftp://ftp.gnu.org/pub/gnu/libiconv")
endif(NOT _correct_iconv)

# ******************************
# Timezone checks
# ******************************

CHECK_C_SOURCE_COMPILES("#include <time.h>
			int main(void) { struct tm tm; tm.tm_gmtoff = 1; return 0; }" HAVE_TM_GMTOFF)

CHECK_C_SOURCE_COMPILES("#include <time.h>
			int main(void) { timezone = 1; return 0; }" HAVE_TIMEZONE)

CHECK_C_SOURCE_COMPILES("#include <time.h>
			int main(void) { altzone = 1; return 0; }" HAVE_ALTZONE)

if((NOT HAVE_TM_GMTOFF) AND (NOT HAVE_TIMEZONE))
	message(FATAL_ERROR "Unable to find a way to determine timezone")
endif((NOT HAVE_TM_GMTOFF) AND (NOT HAVE_TIMEZONE))

# ******************************
# iso-codes
# ******************************

pkg_check_exists(HAVE_ISO_CODES iso-codes)
if(HAVE_ISO_CODES)
	pkg_check_at_least_version(HAVE_ISO_CODES iso-codes 0.49)
	if(HAVE_ISO_CODES)
		pkg_check_variable(_iso_codes_domains iso-codes domains)
		if(NOT ((_iso_codes_domains MATCHES "639") AND (_iso_codes_domains MATCHES "3166")))
			message(WARNING "iso-codes detected, but either iso-639 or iso-3166 domains not found in '${_iso_codes_domains}'")
			set(HAVE_ISO_CODES OFF)
		else()
			pkg_check_variable(ISO_CODES_PREFIX iso-codes prefix)
		endif()
	else(HAVE_ISO_CODES)
		set(HAVE_ISO_CODES OFF)
		message(WARNING "iso-codes detected, but version 0.49 or later is required due to licensing")
	endif(HAVE_ISO_CODES)
endif(HAVE_ISO_CODES)

# ******************************
# libcryptui, aka Seahorse
# ******************************

add_printable_option(ENABLE_LIBCRYPTUI "Enable libcryptui usage" ON)

if(ENABLE_LIBCRYPTUI)
	pkg_check_modules_for_option(ENABLE_LIBCRYPTUI "libcryptui usage" LIBCRYPTUI cryptui-0.0)
	set(HAVE_AUTOAR ON)
endif(ENABLE_LIBCRYPTUI)

# ******************************
# TNEF implementation
# ******************************

add_printable_option(ENABLE_YTNEF "Enable yTNEF library usage" ON)

if(ENABLE_YTNEF)
	set(TNEF_LDFLAGS -lytnef)

	set(CMAKE_REQUIRED_LIBRARIES ${TNEF_LDFLAGS})
	CHECK_C_SOURCE_COMPILES("#include <stdio.h>
				#include <ytnef.h>
				int main(void) { TNEFStruct *tnef; return 0; }" HAVE_YTNEF_H)

	if(NOT HAVE_YTNEF_H)
		CHECK_C_SOURCE_COMPILES("#include <stdio.h>
					#include <libytnef/ytnef.h>
					int main(void) { TNEFStruct *tnef; return 0; }" HAVE_LIBYTNEF_YTNEF_H)
	endif(NOT HAVE_YTNEF_H)
	unset(CMAKE_REQUIRED_LIBRARIES)

	if((NOT HAVE_YTNEF_H) AND (NOT HAVE_LIBYTNEF_YTNEF_H))
		message(FATAL_ERROR "Cannot find ytnef library, either install it or disable use of it by adding -DENABLE_YTNEF=OFF argument to cmake command")
	endif((NOT HAVE_YTNEF_H) AND (NOT HAVE_LIBYTNEF_YTNEF_H))
endif(ENABLE_YTNEF)

# ******************************
# Bogofilter (spam filter)
# ******************************

add_printable_variable_path(WITH_BOGOFILTER "Enable spam filtering using Bogofilter (defaults to /usr/bin/bogofilter)" ON)

string(LENGTH "${CMAKE_BINARY_DIR}" bindirlen)
string(SUBSTRING "${WITH_BOGOFILTER}" 0 ${bindirlen} substr)
string(TOUPPER "${WITH_BOGOFILTER}" optupper)

set(BOGOFILTER_COMMAND "")
if(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))
	set(WITH_BOGOFILTER ON)
elseif(("${optupper}" STREQUAL "OFF") OR ("${optupper}" STREQUAL "NO"))
	set(WITH_BOGOFILTER OFF)
else()
	set(BOGOFILTER_COMMAND "${WITH_BOGOFILTER}")
	set(WITH_BOGOFILTER ON)
endif()

if(WITH_BOGOFILTER)
	if(BOGOFILTER_COMMAND STREQUAL "")
		set(BOGOFILTER_COMMAND "$ENV{BOGOFILTER}")
	endif(BOGOFILTER_COMMAND STREQUAL "")
	if(BOGOFILTER_COMMAND STREQUAL "")
		set(BOGOFILTER_COMMAND "/usr/bin/bogofilter")
	endif(BOGOFILTER_COMMAND STREQUAL "")
endif(WITH_BOGOFILTER)

unset(bindirlen)
unset(substr)
unset(optupper)

# ******************************
# SpamAssassin (spam filter)
# ******************************

add_printable_variable_path(WITH_SPAMASSASSIN "Enable spam filtering using SpamAssassin (defaults to /usr/bin/spamassassin)" ON)
add_printable_variable_path(WITH_SA_LEARN "Full path command where sa-learn is located (defaults to /usr/bin/sa-learn)" ON)

string(LENGTH "${CMAKE_BINARY_DIR}" bindirlen)
string(SUBSTRING "${WITH_SPAMASSASSIN}" 0 ${bindirlen} substr)
string(TOUPPER "${WITH_SPAMASSASSIN}" optupper)

set(SPAMASSASSIN_COMMAND "")
if(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))
	set(WITH_SPAMASSASSIN ON)
elseif(("${optupper}" STREQUAL "OFF") OR ("${optupper}" STREQUAL "NO"))
	set(WITH_SPAMASSASSIN OFF)
else()
	set(SPAMASSASSIN_COMMAND "${WITH_SPAMASSASSIN}")
	set(WITH_SPAMASSASSIN ON)
endif()

if(WITH_SPAMASSASSIN)
	if(SPAMASSASSIN_COMMAND STREQUAL "")
		set(SPAMASSASSIN_COMMAND "$ENV{SPAMASSASSIN}")
	endif(SPAMASSASSIN_COMMAND STREQUAL "")
	if(SPAMASSASSIN_COMMAND STREQUAL "")
		set(SPAMASSASSIN_COMMAND "/usr/bin/spamassassin")
	endif(SPAMASSASSIN_COMMAND STREQUAL "")

	string(SUBSTRING "${WITH_SA_LEARN}" 0 ${bindirlen} substr)
	string(TOUPPER "${WITH_SA_LEARN}" optupper)

	set(SA_LEARN_COMMAND "")
	if(("${optupper}" STREQUAL "ON") OR ("${substr}" STREQUAL "${CMAKE_BINARY_DIR}"))
		set(WITH_SA_LEARN ON)
	elseif(("${optupper}" STREQUAL "OFF") OR ("${optupper}" STREQUAL "NO"))
		set(WITH_SA_LEARN OFF)
	else()
		set(SA_LEARN_COMMAND "${WITH_SA_LEARN}")
		set(WITH_SA_LEARN ON)
	endif()

	if(SA_LEARN_COMMAND STREQUAL "")
		set(SA_LEARN_COMMAND "$ENV{SA_LEARN}")
	endif(SA_LEARN_COMMAND STREQUAL "")
	if(SA_LEARN_COMMAND STREQUAL "")
		set(SA_LEARN_COMMAND "/usr/bin/sa-learn")
	endif(SA_LEARN_COMMAND STREQUAL "")
endif(WITH_SPAMASSASSIN)

unset(bindirlen)
unset(substr)
unset(optupper)

# ******************************
# CERT_UI Flags
# ******************************
#
# Here we want the Mozilla flags to go *before* the other ones,
# especially the mozilla-nss -I flags to go before the gnutls ones,
# as both gnutls and mozilla-nss have a header called "pkcs12.h" which is
# included in smime/lib/e-pkcs12.c. It wants the Mozilla NSS one.

set(CERT_UI_INCLUDES "${MANUAL_NSPR_INCLUDES}")
set(CERT_UI_LIBS "${MANUAL_NSPR_LIBS}")

# ******************************
# Libnotify
# ******************************

pkg_check_modules(LIBNOTIFY libnotify>=${libnotify_minimum_version})
set(HAVE_LIBNOTIFY ${LIBNOTIFY_FOUND})

# ******************************
# libical tweaks
# ******************************

set(CMAKE_REQUIRED_FLAGS "${EVOLUTION_DATA_SERVER_CFLAGS}")
set(CMAKE_REQUIRED_INCLUDES "${EVOLUTION_DATA_SERVER_INCLUDE_DIRS}")
set(CMAKE_REQUIRED_LIBRARIES "${EVOLUTION_DATA_SERVER_LDFLAGS}")

CHECK_C_SOURCE_COMPILES("#include <libical/ical.h>
			int main(void) {
				ical_set_unknown_token_handling_setting (ICAL_DISCARD_TOKEN);
				return 0;
			}" HAVE_ICAL_UNKNOWN_TOKEN_HANDLING)

CHECK_C_SOURCE_COMPILES("#include <libical/ical.h>
			int main(void) {
				icaltzutil_set_exact_vtimezones_support (0);
				return 0;
			}" HAVE_ICALTZUTIL_SET_EXACT_VTIMEZONES_SUPPORT)

unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LIBRARIES)

# ******************************
# gtkspell
# ******************************

add_printable_option(ENABLE_GTKSPELL "Enable gtkspell usage" ON)

if(ENABLE_GTKSPELL)
	pkg_check_modules_for_option(ENABLE_GTKSPELL "gtkspell usage" GTKSPELL gtkspell3-3.0)
	set(HAVE_GTKSPELL ON)
endif(ENABLE_GTKSPELL)

# ******************************
# gnu_get_libc_version()
# ******************************

CHECK_C_SOURCE_COMPILES("#include <gnu/libc-version.h>
			int main(void) { const gchar *libc_version = gnu_get_libc_version (); return 0; }" HAVE_GNU_GET_LIBC_VERSION)

# ******************************
# Plugins
# ******************************

add_printable_variable(ENABLE_PLUGINS "Enable plugins (no/base/all)" "all")

set(plugins_base
	publish-calendar
)
set(plugins_standard
	attachment-reminder
	bbdb
	dbx-import
	email-custom-header
	external-editor
	face
	mail-to-task
	mailing-list-actions
	mail-notification
	save-calendar
	templates
)

# ******************************************************************
# The following plugins have additional library dependencies.
# They must be explicitly disabled if the libraries are not present.
# ******************************************************************

# text-highlight

add_printable_option(ENABLE_TEXT_HIGHLIGHT "Enable text-highlight plugin" ON)

if(ENABLE_TEXT_HIGHLIGHT)
	find_program(HIGHLIGHT_COMMAND highlight)

	if(NOT HIGHLIGHT_COMMAND)
		message(FATAL_ERROR "The 'highlight' program not found; either give it into PATH or disable higlight plugin with -DENABLE_TEXT_HIGHLIGHT=OFF")
	endif(NOT HIGHLIGHT_COMMAND)
endif(ENABLE_TEXT_HIGHLIGHT)

# weather calendar

add_printable_option(ENABLE_WEATHER "Enable weather calendars" ON)

if(ENABLE_WEATHER)
	pkg_check_modules_for_option(ENABLE_WEATHER "weather calendar" GWEATHER gweather-3.0>=${gweather_minimum_version})
endif(ENABLE_WEATHER)

# maps in Contacts preview

add_printable_option(ENABLE_CONTACT_MAPS "Enable contact maps" OFF)

if(ENABLE_CONTACT_MAPS)
	pkg_check_modules_for_option(ENABLE_CONTACT_MAPS "contact maps" CHAMPLAIN champlain-gtk-0.12>=${champlain_minimum_version})
	pkg_check_modules_for_option(ENABLE_CONTACT_MAPS "contact maps" GEO geocode-glib-1.0>=${geocode_glib_minimum_version})
	pkg_check_modules_for_option(ENABLE_CONTACT_MAPS "contact maps" CLUTTER_GTK clutter-gtk-1.0>=${clutter_gtk_minimum_version})
endif(ENABLE_CONTACT_MAPS)

# pst-import plugin

add_printable_option(ENABLE_PST_IMPORT "Enable pst-import plugin" ON)

if(ENABLE_PST_IMPORT)
	pkg_check_modules_for_option(ENABLE_PST_IMPORT "pst-import plugin" LIBPST libpst>=${libpst_minimum_version})
	list(APPEND plugins_standard pst-import)
endif(ENABLE_PST_IMPORT)

# Finish plugins build setup
set(build_plugins)

if(ENABLE_PLUGINS STREQUAL "" OR ENABLE_PLUGINS STREQUAL "all")
	set(build_plugins ${plugins_base} ${plugins_standard})
elseif(ENABLE_PLUGINS STREQUAL "base")
	set(build_plugins ${plugins_base})
elseif(NOT ENABLE_PLUGINS STREQUAL "no")
	message(FATAL_ERROR "Incorrect value for ENABLE_PLUGINS (${ENABLE_PLUGINS}), use either \"no\", or \"base\" or \"all\" value")
endif()

# ******************************
# Glade catalog
# ******************************

add_printable_option(WITH_GLADE_CATALOG "Install the catalog files for Glade 3 (for maintainers only)" OFF)

if(WITH_GLADE_CATALOG)
	pkg_check_modules_for_option(WITH_GLADE_CATALOG "Glade 3 catalog files" GLADEUI gladeui-2.0>=${gladeui_minimum_version})
endif(WITH_GLADE_CATALOG)

# Generate the ${PROJECT_NAME}-config.h file
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config.h)

print_build_options()

# The shell_private_requirements is used by the evolution-shell.pc.in
if(ENABLE_GNOME_DESKTOP)
	set(shell_private_requirements "Requires.private: ${GNOME_DESKTOP_DEPENDENCY}")
else(ENABLE_GNOME_DESKTOP)
	set(shell_private_requirements "")
endif(ENABLE_GNOME_DESKTOP)

add_pkgconfig_file(evolution-calendar.pc.in evolution-calendar-${INTERFACE_VERSION}.pc)
add_pkgconfig_file(evolution-mail.pc.in evolution-mail-${INTERFACE_VERSION}.pc)
add_pkgconfig_file(evolution-shell.pc.in evolution-shell-${INTERFACE_VERSION}.pc)

add_appdata_file(evolution.appdata.xml.in evolution.appdata.xml)

add_subdirectory(data)
add_subdirectory(po)
add_subdirectory(src)
add_subdirectory(tests)

if(ENABLE_GTK_DOC)
	add_subdirectory(docs)
endif(ENABLE_GTK_DOC)

if(WITH_HELP)
	add_subdirectory(help)
endif(WITH_HELP)

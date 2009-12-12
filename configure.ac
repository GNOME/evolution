dnl Evolution Versions
m4_define([evo_major_version], [2])
m4_define([evo_minor_version], [28])
m4_define([evo_micro_version], [3])
m4_define([evo_version],
	[evo_major_version.evo_minor_version.evo_micro_version])
m4_define([evo_stable_version],
	[m4_if(m4_eval(evo_minor_version % 2), [1],
	[evo_major_version.m4_eval(evo_minor_version - 1)],
	[evo_version])])

dnl Base Version: This is for API/version tracking for things like
dnl Bonobo server files.  This should always be the major/minor of
dnl the stable version or stable version to be.  Note, this is set
dnl the way it is so that GETTEXT_PACKAGE will be parsed correctly.
BASE_VERSION=2.28
m4_define([base_version], [2.28])

dnl Upgrade Revision: This is for triggering migration calls between
dnl varying versions. Reset to 0 whenever BASE_VERSION changes.
m4_define([upgrade_revision], [0])

dnl Autoconf / Automake Initialization
AC_PREREQ(2.58)
AC_INIT([evolution],[evo_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution])
AM_INIT_AUTOMAKE([gnu 1.9])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(README)

dnl This is for the autoconf tests only - it set's the language we use
AC_LANG(C)

dnl Automake 1.11 - Silent Build Rules
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Gnome Doc Initialization
GNOME_DOC_INIT

dnl Some requirements have versioned package names
dnl XXX In the spirit of getting rid of versioned
dnl     files, can we please drop these suffixes?
EDS_PACKAGE=1.2
GTKHTML_PACKAGE=3.14

dnl Required Packages
m4_define([glib_minimum_version], [2.20.0])
m4_define([gtk_minimum_version], [2.16.0])
m4_define([eds_minimum_version], [evo_version])
m4_define([gnome_icon_theme_minimum_version], [2.19.91])
m4_define([gnome_desktop_minimum_version], [2.26.0])
m4_define([libbonobo_minimum_version], [2.20.3])
m4_define([libbonoboui_minimum_version], [2.4.2])
m4_define([libgtkhtml_minimum_version], [3.27.90])
m4_define([gconf_minimum_version], [2.0.0])		dnl XXX Just a Guess
m4_define([libglade_minimum_version], [2.0.0])		dnl XXX Just a Guess
m4_define([libgnomecanvas_minimum_version], [2.0.0])	dnl XXX Just a Guess
m4_define([libgnomeui_minimum_version], [2.0.0])	dnl XXX Just a Guess
m4_define([libxml_minimum_version], [2.7.3])
m4_define([shared_mime_info_minimum_version], [0.22])
m4_define([libpst_minimum_version], [0.6.41])

dnl Optional Packages
m4_define([dbus_minimum_version], [0.74])
m4_define([nm_minimum_version],[0.7])
m4_define([hal_minimum_version], [0.5.4])
m4_define([libnotify_minimum_version], [0.3.0])
m4_define([gnome_pilot_minimum_version], [2.0.15])
m4_define([gweather_minimum_version], [2.25.3])

dnl Compiler Warning Flags

AS_COMPILER_FLAGS(WARNING_FLAGS,
	"-DG_DISABLE_DEPRECATED
	-DPANGO_DISABLE_DEPRECATED
	-DGDK_PIXBUF_DISABLE_DEPRECATED
	-DGDK_DISABLE_DEPRECATED
	-DG_DISABLE_SINGLE_INCLUDES
	-DGTK_DISABLE_SINGLE_INCLUDES
	-Wall -Wextra
	-Wno-missing-field-initializers
	-Wno-sign-compare
	-Wno-unused-parameter
	-Wdeclaration-after-statement
	-Werror-implicit-function-declaration
	-Wformat-security -Winit-self
	-Wmissing-declarations -Wmissing-include-dirs
	-Wmissing-noreturn -Wnested-externs -Wpointer-arith
	-Wredundant-decls -Wundef -Wwrite-strings")
AC_SUBST(WARNING_FLAGS)

dnl Other useful compiler warnings for test builds only.
dnl These may produce warnings we have no control over.
dnl	-Wmissing-format-attribute
dnl	-Wshadow
dnl	-Wformat-nonliteral

CFLAGS="$CFLAGS $WARNING_FLAGS"

AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(VERSION_COMMENT, "", [Define if you want a comment appended to the version number])

dnl Put the ACLOCAL flags in the Makefile
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

dnl Initialize maintainer mode
AM_MAINTAINER_MODE

if test "$evo_version" != "$evo_stable_version"; then
	AC_DEFINE(STABLE_VERSION, ["evo_stable_version"],
		[Define to the latest stable version if this version is unstable])
fi

AC_SUBST([BASE_VERSION], [base_version])
AC_DEFINE_UNQUOTED(BASE_VERSION, ["$BASE_VERSION"], [Base version (Major.Minor)])

AC_SUBST([UPGRADE_REVISION],[upgrade_revision])
AC_DEFINE_UNQUOTED(UPGRADE_REVISION, ["$UPGRADE_REVISION"], [The number of times we've upgraded since the BASE_VERSION release])

AC_PROG_CC
AC_C_INLINE
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_AWK
AC_PATH_PROG([PERL], [perl])
AC_SUBST(PERL)

case $YACC in
*yacc*)
	AC_MSG_ERROR([You need bison to build Evolution])
	;;
esac
AC_ARG_PROGRAM

dnl  Test whether jw is installed
AC_PATH_PROG(JW,jw,no)
if test x$JW = xno; then
	HAVE_JW="no"
else
	HAVE_JW="yes"
fi
AM_CONDITIONAL(HAVE_JW, [test x$HAVE_JW = xyes])
AC_SUBST(HAVE_JW)

dnl I18N stuff
IT_PROG_INTLTOOL([0.35.5])

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=evolution-$BASE_VERSION
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

localedir='$(prefix)/$(DATADIRNAME)/locale'
AC_SUBST(localedir)

dnl Initialize libtool
dnl if switch to libtool >= 2.2, replace existing with following:
dnl LT_PREREQ(2.2)
dnl LT_INIT(disable-static win32-dll)
AM_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL
DOLT

PKG_PROG_PKG_CONFIG

dnl Win32
AC_MSG_CHECKING([for Win32])
case "$host" in
*-mingw*)
	os_win32=yes
	NO_UNDEFINED='-no-undefined'
	SOEXT='.dll'
	SA_JUNK_PLUGIN=''
	BF_JUNK_PLUGIN=''
	DL_LIB=''
	SOFTOKN3_LIB=''
	HAL_REQUIREMENT=''
	;;
*)
	os_win32=no
	NO_UNDEFINED=''
	SOEXT='.so'
	SA_JUNK_PLUGIN=sa-junk-plugin
	BF_JUNK_PLUGIN=bogo-junk-plugin
	DL_LIB='-ldl'
	SOFTOKN3_LIB='-lsoftokn3'
	HAL_REQUIREMENT='hal'
	;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, [test $os_win32 = yes])
AC_SUBST(NO_UNDEFINED)
AC_SUBST(SOEXT)

if test "$os_win32" = "yes"; then
	AC_CHECK_TOOL(WINDRES, windres, :)
else
	WINDRES=":"
fi

AM_CONDITIONAL(HAVE_WINDRES, test "x$WINDRES" != "x:")

AC_SUBST(WINDRES)

dnl Check for base dependencies early.
PKG_CHECK_MODULES([GNOME_PLATFORM],
	[glib-2.0 >= glib_minimum_version
	gtk+-2.0 >= gtk_minimum_version
	gconf-2.0 >= gconf_minimum_version
	libbonobo-2.0 >= libbonobo_minimum_version
	libbonoboui-2.0 >= libbonoboui_minimum_version
	libglade-2.0 >= libglade_minimum_version
	libgnomecanvas-2.0 >= libgnomecanvas_minimum_version
	libgnomeui-2.0 >= libgnomeui_minimum_version
	libxml-2.0 >= libxml_minimum_version
	shared-mime-info >= shared_mime_info_minimum_version
	gnome-desktop-2.0 >= gnome_desktop_minimum_version])
AC_SUBST(GNOME_PLATFORM_CFLAGS)
AC_SUBST(GNOME_PLATFORM_LIBS)

PKG_CHECK_MODULES([EVOLUTION_DATA_SERVER],
	[libebook-$EDS_PACKAGE >= eds_minimum_version
	libecal-$EDS_PACKAGE >= eds_minimum_version
	libedataserver-$EDS_PACKAGE >= eds_minimum_version
	libedataserverui-$EDS_PACKAGE >= eds_minimum_version
	libegroupwise-$EDS_PACKAGE >= eds_minimum_version
	libebackend-$EDS_PACKAGE >= eds_minimum_version
	libgdata-$EDS_PACKAGE >= eds_minimum_version
	libgdata-google-$EDS_PACKAGE >= eds_minimum_version])


dnl ******************
dnl User documentation
dnl ******************
AC_MSG_CHECKING([whether to build user documentation])
AC_ARG_WITH([help],
	[AS_HELP_STRING([--with-help],
	[Build user documentation [default=yes]])],
	[with_help="$withval"],[with_help="yes"])
AM_CONDITIONAL(WITH_HELP, test "x$with_help" != "xno")
AC_MSG_RESULT([$with_help])

dnl ******************************
dnl iconv checking
dnl ******************************
have_iconv=no
save_LIBS="$LIBS"
LIBS="$LIBS -liconv"
AC_CACHE_CHECK([for iconv in -liconv], [ac_cv_libiconv],
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[[
		#include <iconv.h>
		#include <stdlib.h>
	]],
	[[	iconv_t cd;
		cd = iconv_open ("UTF-8", "ISO-8859-1");
	]]
	)],[ac_cv_libiconv=yes],[ac_cv_libiconv=no]))

if test $ac_cv_libiconv = yes; then
	ICONV_LIBS="-liconv"
	if test $os_win32 = yes; then
		dnl Don't pointlessly auto-export the global symbols
		dnl from a potentially static libiconv.a
		ICONV_LIBS="$ICONV_LIBS -Wl,--exclude-libs=libiconv.a"
	fi
	have_iconv=yes
else
	LIBS="$save_LIBS"
	AC_CHECK_FUNC(iconv, have_iconv=yes, have_iconv=no)
fi

if test $have_iconv = yes; then
	if test $ac_cv_libiconv = no; then
		AC_CHECK_FUNCS(gnu_get_libc_version)
	fi
AC_CACHE_CHECK([if iconv() handles UTF-8], [ac_cv_libiconv_utf8],
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
		#include <iconv.h>
		#include <stdlib.h>
		#include <string.h>
		#ifdef HAVE_GNU_GET_LIBC_VERSION
		#include <gnu/libc-version.h>
		#endif
		int main() {
			char *jp = "\x1B\x24\x42\x46\x7C\x4B\x5C\x38\x6C";
			char *utf8 = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
			char *transbuf = malloc (10), *trans = transbuf;
			iconv_t cd;
			size_t jp_len = strlen (jp), utf8_len = 10;
			size_t utf8_real_len = strlen (utf8);

			#ifdef HAVE_GNU_GET_LIBC_VERSION
			/* glibc 2.1.2's iconv is broken in hard to test ways. */
			if (!strcmp (gnu_get_libc_version (), "2.1.2"))
				exit (1);
			#endif

			cd = iconv_open ("UTF-8", "ISO-2022-JP");
			if (cd == (iconv_t) -1)
				exit (1);
			if (iconv (cd, &jp, &jp_len, &trans, &utf8_len) == -1 || jp_len != 0)
				exit (1);
			if (memcmp (utf8, transbuf, utf8_real_len) != 0)
				exit (1);
			return (0);}
		]])
	],[ac_cv_libiconv_utf8=yes],[ac_cv_libiconv_utf8=no; have_iconv=no],[ac_cv_libiconv_utf8=hopefully]))
fi

if test "$have_iconv" = no; then
	AC_MSG_ERROR([You need to install a working iconv implementation, such as ftp://ftp.gnu.org/pub/gnu/libiconv])
fi
AC_SUBST(ICONV_LIBS)

CFLAGS="$CFLAGS -I$srcdir"

AC_MSG_CHECKING([preferred charset name formats for system iconv])
AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#define CONFIGURE_IN
	#include "iconv-detect.c"
	]])],
	[AC_MSG_RESULT([found])],
	[AC_MSG_RESULT([not found])
		AC_MSG_WARN([
		*** The iconv-detect program was unable to determine the
		*** preferred charset name formats recognized by your
		*** iconv library. It is suggested that you install a
		*** working iconv library such as the one found at
		*** ftp://ftp.gnu.org/pub/gnu/libiconv
	])],
	[if test x$os_win32 = xyes; then
		AC_MSG_RESULT([using known win32 result])
		echo '/* This is an auto-generated header, DO NOT EDIT! */' > iconv-detect.h
		echo  >>iconv-detect.h
		echo '#define ICONV_ISO_D_FORMAT "iso-%d-%d"' >>iconv-detect.h
		echo '#define ICONV_ISO_S_FORMAT "iso-%d-%s"' >>iconv-detect.h
		echo '#define ICONV_10646 "UCS-4BE"' >>iconv-detect.h
	else
		AC_MSG_RESULT([unknown])
		AC_MSG_WARN([
		*** We can't determine the preferred charset name formats
		*** recognized by your iconv library. You are
		*** cross-compiling and supposed to know what you are doing.
		*** Please construct the iconv-detect.h file manually.
		])
	fi
])

CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

dnl alloca()
AC_FUNC_ALLOCA

dnl waitpid() etc
AC_HEADER_SYS_WAIT

dnl
dnl Purify support
dnl
EVO_PURIFY_SUPPORT

dnl **************
dnl Test Component
dnl **************
AC_ARG_ENABLE([test-component],
	AS_HELP_STRING([--enable-test-component],
	[Enable test component.]),
	[enable_test_comp=$enableval],[enable_test_comp=no])

AM_CONDITIONAL(ENABLE_TEST_COMPONENT, [test x$enable_test_comp = xyes])

dnl ***************
dnl Timezone checks
dnl ***************
AC_CACHE_CHECK(for tm_gmtoff in struct tm, [ac_cv_struct_tm_gmtoff],
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[	#include <time.h>	]],
	[[
		struct tm tm;
		tm.tm_gmtoff = 1;
	]]
	)],[ac_cv_struct_tm_gmtoff=yes],[ac_cv_struct_tm_gmtoff=no]))
if test $ac_cv_struct_tm_gmtoff = yes; then
	AC_DEFINE(HAVE_TM_GMTOFF, 1, [Define if struct tm has a tm_gmtoff member])
else
	AC_CACHE_CHECK(for timezone variable, [ac_cv_var_timezone],
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
		[[	#include <time.h> ]],
		[[	timezone = 1;	]]
		)],[ac_cv_var_timezone=yes],[ac_cv_var_timezone=no]))
	if test $ac_cv_var_timezone = yes; then
		AC_DEFINE(HAVE_TIMEZONE, 1, [Define if libc defines a timezone variable])
		AC_CACHE_CHECK(for altzone variable, [ac_cv_var_altzone],
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
			[[	#include <time.h>	]],
			[[	altzone = 1; ]]
			)],[ac_cv_var_altzone=yes],[ac_cv_var_altzone=no]))
		if test $ac_cv_var_altzone = yes; then
			AC_DEFINE(HAVE_ALTZONE, 1, [Define if libc defines an altzone variable])
		fi
	else
		AC_MSG_ERROR([unable to find a way to determine timezone])
	fi
fi


AC_CHECK_FUNCS(mkstemp mkdtemp isblank)

dnl **************************************************
dnl ctime_r prototype
dnl **************************************************

AC_CACHE_CHECK([if ctime_r wants three arguments], [ac_cv_ctime_r_three_args],
[
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[	#include <time.h> ]],
	[[	char *buf;
		time_t date;
		ctime_r (&date, buf, 100); ]]
	)],[ac_cv_ctime_r_three_args=yes],[ac_cv_ctime_r_three_args=no])
])

if test x"$ac_cv_ctime_r_three_args" = xyes ; then
	AC_DEFINE(CTIME_R_THREE_ARGS, 1, [Solaris-style ctime_r])
fi

dnl **************************************************
dnl gethostbyname_r prototype
dnl **************************************************

AC_CHECK_FUNCS(gethostbyname_r,[
AC_CACHE_CHECK([if gethostbyname_r wants five arguments], [ac_cv_gethostbyname_r_five_args],
[
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[
		#include "confdefs.h"
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <netdb.h>

		#define BUFSIZE (sizeof(struct hostent)+10)
	]],
	[[
		struct hostent hent;
		char buffer[BUFSIZE];
		int bufsize=BUFSIZE;
		int h_errno;
		(void)gethostbyname_r ("www.ximian.com", &hent, buffer, bufsize, &h_errno);
	]]
	)],[ac_cv_gethostbyname_r_five_args=yes],[ac_cv_gethostbyname_r_five_args=no])
])])

if test "x$ac_cv_gethostbyname_r_five_args" = "xyes" ; then
	AC_DEFINE(GETHOSTBYNAME_R_FIVE_ARGS, 1, [Solaris-style gethostbyname_r])
fi

dnl **************************************************
dnl gethostbyaddr_r prototype
dnl **************************************************

AC_CHECK_FUNCS(gethostbyaddr_r,[
AC_CACHE_CHECK([if gethostbyaddr_r wants seven arguments], [ac_cv_gethostbyaddr_r_seven_args],
[
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[
		#include "confdefs.h"
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <netdb.h>
		#define BUFSIZE (sizeof(struct hostent)+10)
	]],
	[[
		struct hostent hent;
		char buffer[BUFSIZE];
		int bufsize=BUFSIZE;
		int h_errno;
		(void)gethostbyaddr_r ("www.ximian.com", 14, AF_INET, &hent, buffer, bufsize, &h_errno); ]]
	)],[ac_cv_gethostbyaddr_r_seven_args=yes],[ac_cv_gethostbyaddr_r_seven_args=no])
])])

if test "x$ac_cv_gethostbyaddr_r_seven_args" = "xyes" ; then
	AC_DEFINE(GETHOSTBYADDR_R_SEVEN_ARGS, 1, [Solaris-style gethostbyaddr_r])
fi

dnl **************************************************
dnl stat(v)fs location/type
dnl **************************************************

AC_CHECK_HEADER([sys/statvfs.h],
	[AC_DEFINE([HAVE_SYS_STATVFS_H], 1, [Have <sys/statvfs.h>])],,
	[[	#if HAVE_SYS_STATVFS_H
		#include <sys/statvfs.h>
		#endif
	]])
AC_CHECK_FUNCS(statvfs)

AC_CHECK_HEADER([sys/param.h],
	[AC_DEFINE([HAVE_SYS_PARAM_H], 1, [Have <sys/param.h>])],,
	[[	#if HAVE_SYS_PARAM_H
		#include <sys/param.h>
		#endif
	]])
AC_CHECK_HEADER([sys/mount.h],
	[AC_DEFINE([HAVE_SYS_MOUNT_H], 1, [Have <sys/mount.h>])],,
	[[	#if HAVE_SYS_MOUNT_H
		#include <sys/mount.h>
		#endif
	]])
AC_CHECK_FUNCS(statfs)

dnl **************************************************
dnl * Gnome Icon Theme
dnl **************************************************
PKG_CHECK_MODULES([GIT], [gnome-icon-theme >= gnome_icon_theme_minimum_version])

dnl **************************************************
dnl * Accessibility support
dnl **************************************************
PKG_CHECK_MODULES([A11Y], [atk])
AC_SUBST(A11Y_CFLAGS)
AC_SUBST(A11Y_LIBS)

dnl **************************************************
dnl * XF86 multimedia keys support
dnl **************************************************
have_xfree=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[ #include <X11/XF86keysym.h> ]],[[]])],
	[have_xfree=yes]
)
AC_MSG_CHECKING([for X11 XFree86 headers])
AC_MSG_RESULT([$have_xfree])
if test x"$have_xfree" = "xyes" ; then
	AC_DEFINE(HAVE_XFREE, 1, [defined if you have X11/XF86keysym.h])
fi

dnl **************************************************
dnl LDAP support.
dnl **************************************************
if test "$os_win32" != yes; then
EVO_LDAP_CHECK(no)
case $with_openldap in
no)
	msg_ldap=no
	;;
*)
	case $with_static_ldap in
	yes)
		msg_ldap="yes (static)"
		;;
	*)
		msg_ldap="yes (dynamic)"
		;;
	esac
esac

SAVE_CFLAGS="$CFLAGS"
SAVE_LIBS="$LIBS"
LDAP_CFLAGS="$LDAP_CFLAGS -DLDAP_DEPRECATED"
CFLAGS="$CFLAGS $LDAP_CFLAGS"
LIBS="$LIBS $LDAP_LIBS"
AC_CHECK_FUNCS(ldap_ntlm_bind)
CFLAGS="$SAVE_CFLAGS"
LIBS="$SAVE_LIBS"

else dnl Win32
LDAP_CFLAGS="-DLDAP_DEPRECATED"
LDAP_LIBS="-lwldap32"
AC_SUBST(LDAP_CFLAGS)
AC_SUBST(LDAP_LIBS)
AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])
AM_CONDITIONAL(ENABLE_LDAP, true)
msg_ldap="yes"
fi dnl Win32

dnl **************************************************
dnl SunLDAP support, but if OpenLDAP support, skip check SunLDAP.
dnl **************************************************
if test "$with_openldap" = no ; then
	EVO_SUNLDAP_CHECK(no)
	case $with_sunldap in
	no)
		msg_ldap=no
		;;
	*)
		case $with_static_sunldap in
		yes)
			msg_ldap="$with_sunldap (static)"
			;;
		*)
			msg_ldap="$with_sunldap (dynamic)"
			;;
		esac
	esac
else
	with_sunldap=no
fi
AM_CONDITIONAL(SUNLDAP, [test x$with_sunldap != xno])

dnl **************************************************
dnl NNTP support.
dnl **************************************************
AC_ARG_ENABLE([nntp],
	AS_HELP_STRING([--enable-nntp],
	[Build Usenet news (NNTP) backend]),
	[enable_nntp=$enableval],[enable_nntp=yes])
if test "x$enable_nntp" = "xyes"; then
	AC_DEFINE(ENABLE_NNTP,1,[Build NNTP backend])
	msg_nntp=yes
else
	msg_nntp=no
fi
AM_CONDITIONAL(ENABLE_NNTP, [test x$enable_nntp = xyes])

AC_DEFINE(HANDLE_LIBICAL_MEMORY, 1, [Define it once memory returned by libical is free'ed properly])

dnl **************************************************
dnl * Posix thread support
dnl **************************************************

dnl GLIB_CONFIG=${GLIB_CONFIG-glib-config}
dnl GNOME_PTHREAD_CHECK

dnl if test "x$PTHREAD_LIB" = "x" ; then
dnl 	AC_MSG_ERROR([POSIX threads are currently required for Evolution])
dnl fi

dnl
dnl Notice that this is a hack, and we wont be able to use this forever, but
dnl at least for some time
dnl

EVO_PTHREAD_CHECK

THREADS_LIBS="$PTHREAD_LIB"
THREADS_CFLAGS="$PTHREAD_CFLAGS"

AC_SUBST(THREADS_LIBS)
AC_SUBST(THREADS_CFLAGS)
AC_DEFINE(ENABLE_THREADS,1,[Required])


dnl *********
dnl Libraries
dnl *********

AC_CHECK_FUNCS(regexec,,[AC_CHECK_LIB(regex,regexec,
			[REGEX_LIBS=-lregex
			AC_DEFINE(HAVE_REGEXEC,1,[Define to 1 if you have the regexec function.])],
			[AC_MSG_ERROR([No regex library found])])])
AC_SUBST(REGEX_LIBS)

dnl XXX Drop the version from the package name?
PKG_CHECK_MODULES([GTKHTML], [libgtkhtml-3.14 >= libgtkhtml_minimum_version])
AC_SUBST(GTKHTML_CFLAGS)
AC_SUBST(GTKHTML_LIBS)
GTKHTML_DATADIR=`$PKG_CONFIG --variable gtkhtml_datadir libgtkhtml-3.14`
AC_SUBST(GTKHTML_DATADIR)
GTKHTML_API_VERSION=`$PKG_CONFIG --variable gtkhtml_apiversion libgtkhtml-3.14`
AC_DEFINE_UNQUOTED(GTKHTML_API_VERSION, "$GTKHTML_API_VERSION", [The gtkhtml api version])

dnl ******************************
dnl Pilot checking
dnl ******************************
AC_ARG_ENABLE([pilot-conduits],
	AS_HELP_STRING([--enable-pilot-conduits],
	[Enable support for building pilot conduits.]),,
	[enable_pilot_conduits=no])
if test "x$enable_pilot_conduits" = "xyes"; then
	PKG_CHECK_MODULES([GNOME_PILOT], [gnome-pilot-2.0 >= gnome_pilot_minimum_version])

	CFLAGS_save="$CFLAGS"
	CFLAGS="$CFLAGS $GNOME_PILOT_CFLAGS"
	LDFLAGS_save="$LDFLAGS"
	LDFLAGS="$GNOME_PILOT_LIBS $LDFLAGS"

	AC_CACHE_CHECK([if pilot-link handles UTF-8 conversions],[ac_cv_pilot_link_utf8],
	AC_RUN_IFELSE([AC_LANG_SOURCE(
	[[	#include <stdlib.h>
		#include <string.h>
		#include <pi-util.h>

		int main (){
		const char *utf8 = "\x66\x66\x66\x66\x66\x66\x66\xC2\xA9";
		size_t utf8_real_len = strlen (utf8);
		char *pstring;
		if (convert_ToPilotChar ("UTF-8", utf8, utf8_real_len, &pstring) == -1)
			exit (1);
		return (0);
		}
	]]
	)],[ac_cv_pilot_link_utf8=yes],[ac_cv_pilot_link_utf8=no],[ac_cv_pilot_link_utf8=no]))

	CFLAGS="$CFLAGS_save"
	LDFLAGS="$LDFLAGS_save"

	if test "$ac_cv_pilot_link_utf8" = no; then
		AC_MSG_ERROR([evolution requires pilot-link to have working UTF-8 conversion routines])
	fi
fi
AM_CONDITIONAL(ENABLE_PILOT_CONDUITS, [test x$enable_pilot_conduits = xyes])
dnl ******************************
dnl If pilot conduits are enabled, check version of pilot-link
dnl ******************************
if test x$enable_pilot_conduits = xyes; then
	msg_pilot=yes

	PILOT_LINK_CHECK(0.11.4)
	AC_SUBST(PILOT_LINK_MAJOR)
	AC_SUBST(PILOT_LINK_MINOR)
	AC_SUBST(PILOT_LINK_MICRO)
	AC_SUBST(PILOT_LINK_PATCH)
	AC_SUBST(PILOT_LINK_VERSION)

	if test $PILOT_LINK_MINOR -ge 12; then
		AC_DEFINE(PILOT_LINK_0_12,,[Building against pilot-link 0.12.0 or greater])
	fi
else
	msg_pilot=no
fi
AC_SUBST(GNOME_PILOT_CFLAGS)
AC_SUBST(GNOME_PILOT_LIBS)

dnl ********
dnl Kerberos
dnl ********
AC_ARG_WITH([krb5],
	AS_HELP_STRING([--with-krb5=PATH],
	[Location of Kerberos 5 install dir]),
	[with_krb5=$withval],[with_krb5=no])
AC_ARG_WITH([krb5-libs],
	AS_HELP_STRING([--with-krb5-libs=PATH],
	[Location of Kerberos 5 libraries]),
	[with_krb5_libs=$withval],[with_krb5_libs=$with_krb5/lib])
AC_ARG_WITH([krb5-includes],
	AS_HELP_STRING([--with-krb5-includes=PATH],
	[Location of Kerberos 5 headers]),
	[with_krb5_includes=$withval],[with_krb5_includes=""])
AC_ARG_WITH([krb4],
	AS_HELP_STRING([--with-krb4=PATH],
	[Location of Kerberos 4 install dir]),
	[with_krb4=$withval],[with_krb4=no])
AC_ARG_WITH([krb4-libs],
	AS_HELP_STRING([--with-krb4-libs=PATH],
	[Location of Kerberos 4 libraries]),
	[with_krb4_libs=$withval],[with_krb4_libs=$with_krb4/lib])
AC_ARG_WITH([krb4-includes],
	AS_HELP_STRING([--with-krb4-includes=PATH],
	[Location of Kerberos 4 headers]),
	[with_krb4_includes=$withval],[with_krb4_includes=""])

msg_krb5="no"
if test "x${with_krb5}" != "xno"; then
	LIBS_save="$LIBS"

	mitlibs="-lkrb5 -lk5crypto -lcom_err -lgssapi_krb5"
	heimlibs="-lkrb5 -lcrypto -lasn1 -lcom_err -lroken -lgssapi"
	sunlibs="-lkrb5 -lgss"
	AC_CACHE_CHECK([for Kerberos 5], [ac_cv_lib_kerberos5],
	[
		LIBS="$LIBS -L$with_krb5_libs $mitlibs"
		AC_LINK_IFELSE([AC_LANG_CALL([], [krb5_init_context])],
		[ac_cv_lib_kerberos5="$mitlibs"],
		[
			LIBS="$LIBS_save -L$with_krb5_libs $heimlibs"
			AC_LINK_IFELSE([AC_LANG_CALL([], [krb5_init_context])],
			[ac_cv_lib_kerberos5="$heimlibs"],
			[
				LIBS="$LIBS_save -L$with_krb5_libs $sunlibs"
				AC_LINK_IFELSE([AC_LANG_CALL([], [krb5_init_context])],
				[ac_cv_lib_kerberos5="$sunlibs"], [ac_cv_lib_kerberos5="no"])
			])
		])
		LIBS="$LIBS_save"
	])
	if test "$ac_cv_lib_kerberos5" != "no"; then
		AC_DEFINE(HAVE_KRB5,1,[Define if you have Krb5])
		if test "$ac_cv_lib_kerberos5" = "$mitlibs"; then
			AC_DEFINE(HAVE_MIT_KRB5,1,[Define if you have MIT Krb5])
			if test -z "$with_krb5_includes"; then
				KRB5_CFLAGS="-I$with_krb5/include"
			else
				KRB5_CFLAGS="-I$with_krb5_includes"
			fi
			msg_krb5="yes (MIT)"
		else
			if test "$ac_cv_lib_kerberos5" = "$heimlibs"; then
				AC_DEFINE(HAVE_HEIMDAL_KRB5,1,[Define if you have Heimdal])
				if test -z "$with_krb5_includes"; then
					KRB5_CFLAGS="-I$with_krb5/include/heimdal"
				else
					KRB5_CFLAGS="-I$with_krb5_includes"
				fi
				msg_krb5="yes (Heimdal)"
			else
				AC_DEFINE(HAVE_SUN_KRB5,1,[Define if you have Sun Kerberosv5])
				if test -z "$with_krb5_includes"; then
					KRB5_CFLAGS="-I$with_krb5/include/kerberosv5"
				else
					KRB5_CFLAGS="-I$with_krb5_includes"
				fi
				msg_krb5="yes (Sun)"
			fi
		fi
		KRB5_LIBS="-L$with_krb5_libs $ac_cv_lib_kerberos5"
	fi
else
	AC_MSG_CHECKING([for Kerberos 5])
	AC_MSG_RESULT([$with_krb5])
fi

AC_CHECK_HEADER([et/com_err.h],
	[AC_DEFINE([HAVE_ET_COM_ERR_H], 1, [Have <et/comm_err.h>])],,
	[[	#if HAVE_ET_COM_ERR_H
		#include <com_err.h>
		#endif
	]])
AC_CHECK_HEADER([com_err.h],
	[AC_DEFINE([HAVE_COM_ERR_H], 1, [Have <comm_err.h>])],,
	[[	#if HAVE_COM_ERR_H
		#include <com_err.h>
		#endif
	]])

msg_krb4="no"
if test "x${with_krb4}" != "xno"; then
	LIBS_save="$LIBS"
	AC_CACHE_CHECK([for Kerberos 4], [ac_cv_lib_kerberos4],
	[
		ac_cv_lib_kerberos4="no"
		mitcompatlibs="-lkrb4 -ldes425 -lkrb5 -lk5crypto -lcom_err"
		dnl Look for MIT krb5 compat krb4
		LIBS="$LIBS_save -L$with_krb4_libs $mitcompatlibs"
		AC_LINK_IFELSE([AC_LANG_CALL([], [krb_mk_req])],
		[ac_cv_lib_kerberos4="$mitcompatlibs"])

		if test "$ac_cv_lib_kerberos4" = "no"; then
			dnl Look for KTH krb4
			LIBS="$LIBS_save -L$with_krb4_libs -lkrb -lcrypto -lcom_err -lroken"
			AC_LINK_IFELSE([AC_LANG_CALL([], [krb_mk_req])],
			[ac_cv_lib_kerberos4="-lkrb -lcrypto -lcom_err -lroken"])
		fi
		if test "$ac_cv_lib_kerberos4" = "no"; then
			dnl Look for old MIT krb4
			LIBS="$LIBS_save -L$with_krb4_libs -lkrb"
			AC_LINK_IFELSE([AC_LANG_CALL([], [krb_mk_req])],
			[ac_cv_lib_kerberos4="-lkrb"],
			[
				LIBS="$LIBS_save -ldes"
				AC_LINK_IFELSE([AC_LANG_CALL([], [krb_mk_req])],
				[ac_cv_lib_kerberos4="-lkrb -ldes"])
			])
		fi
	])
	LIBS="$LIBS_save"
	if test "$ac_cv_lib_kerberos4" != "no"; then
		AC_DEFINE(HAVE_KRB4,1,[Define if you have Krb4])
		msg_krb4="yes"

		if test -z "$with_krb4_includes"; then
			if test -f "$with_krb4/include/krb.h" -o -f "$with_krb4/include/port-sockets.h"; then
				KRB4_CFLAGS="-I$with_krb4/include"
			fi
			if test -d "$with_krb4/include/kerberosIV"; then
				KRB4_CFLAGS="$KRB4_CFLAGS -I$with_krb4/include/kerberosIV"
			fi
		else
			KRB4_CFLAGS="-I$with_krb4_includes"
		fi
		KRB4_LIBS="-L$with_krb4_libs $ac_cv_lib_kerberos4"

		CFLAGS_save="$CFLAGS"
		CFLAGS="$CFLAGS $KRB4_CFLAGS"
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
		[[	#include "krb.h"
			int krb_sendauth;
		]]
		,[[]])],[AC_DEFINE(NEED_KRB_SENDAUTH_PROTO,1,[Need krb_sendauth proto])],[])
		CFLAGS="$CFLAGS_save"
	fi
else
	AC_MSG_CHECKING([for Kerberos 4])
	AC_MSG_RESULT([${with_krb4}])
fi

AC_SUBST(KRB5_CFLAGS)
AC_SUBST(KRB5_LIBS)
AC_SUBST(KRB4_CFLAGS)
AC_SUBST(KRB4_LIBS)

dnl Mono hooks
dnl This should just define mono CFLAGS etc here, it is used later to
dnl turn on the mono plugin or not.

MONO_CFLAGS=
MONO_LIBS=
AC_ARG_ENABLE([mono],
	AS_HELP_STRING([--enable-mono],
	[Add Mono embedded hooks.]),
	[enable_mono=$enableval],[enable_mono=no])
if test "x${enable_mono}" = "xyes"; then
	PKG_CHECK_MODULES([MONO], ["mono"])
	AC_DEFINE(ENABLE_MONO,1,[Define if Mono embedding should be enabled])
	MONO_PLUGIN="mono"
fi
AC_SUBST(MONO_CFLAGS)
AC_SUBST(MONO_LIBS)

dnl Python hooks
dnl This should just define python CFLAGS etc here, it is used later to
dnl turn on the python plugin or not.
dnl (Thanks to Pidgin)

AC_ARG_ENABLE([python],
	AS_HELP_STRING([--enable-python],
	[Add python embedded hooks.]),
	[enable_python=$enableval],[enable_python=no])

if test "x${enable_python}" = "xyes"; then
	AC_PATH_PROG(pythonpath, python)
	if test "_$pythonpath" != _ ; then
		AC_MSG_CHECKING([for python compile flags])
		PY_PREFIX=`$pythonpath -c 'import sys ; print sys.prefix'`
		PY_EXEC_PREFIX=`$pythonpath -c 'import sys ; print sys.exec_prefix'`
		changequote(<<, >>)dnl
		PY_VERSION=`$pythonpath -c 'import sys ; print sys.version[0:3]'`
		PY_MAJOR=`$pythonpath -c 'import sys ; print sys.version[0:2]'`
		changequote([, ])dnl

		if test -f $PY_PREFIX/include/python$PY_VERSION/Python.h -a "$PY_MAJOR" = "2."; then
		   	PY_LIBS="-lpython$PY_VERSION -L$PY_EXEC_PREFIX/lib/python$PY_VERSION/config"
			PY_INCLUDES="-I$PY_PREFIX/include/python$PY_VERSION"
			AC_MSG_RESULT([ok])
			python_package="python-devel"
			PYTHON_PLUGIN="python"
		else
			AC_MSG_ERROR([Can't find Python.h])
			PY_LIBS=""
			PY_INCLUDES=""
			python_package=""
			PYTHON_PLUGIN=""
		fi
	fi
	AC_SUBST(PY_LIBS)
	AC_SUBST(PY_INCLUDES)
fi


dnl ********************************************************************************
dnl security extension support (SSL and S/MIME)
dnl
dnl The following voodoo does detection of mozilla libraries (nspr and nss)
dnl needed by Camel (SSL and S/MIME).
dnl
dnl The Evolution security extensions are only built if these libraries are found
dnl ********************************************************************************
msg_ssl="no"
msg_smime="no"

dnl these 2 enable's are inverses of each other

AC_ARG_ENABLE([nss],
	AS_HELP_STRING([--enable-nss=@<:@yes/no/static@:>@],
	[Attempt to use Mozilla libnss for SSL support.]),
	[enable_nss=$enableval],[enable_nss=yes])

AC_ARG_ENABLE([smime],
	AS_HELP_STRING([--enable-smime],
	[Attempt to use Mozilla libnss for SMIME support (this requires --enable-nss)]),
	[enable_smime=$enableval],[enable_smime=yes])

AC_ARG_WITH([nspr-includes],
	AS_HELP_STRING([--with-nspr-includes=PATH],
	[Location of Mozilla nspr4 includes.]))

AC_ARG_WITH([nspr-libs],
	AS_HELP_STRING([--with-nspr-libs=PATH],
	[Location of Mozilla nspr4 libs.]))

AC_ARG_WITH([nss-includes],
	AS_HELP_STRING([--with-nss-includes=PATH],
	[Location of Mozilla nss3 includes.]))

AC_ARG_WITH([nss-libs],
	AS_HELP_STRING([--with-nss-libs=PATH],
	[Location of Mozilla nss3 libs.]))

if test "x${enable_nss}" = "xyes" || test "x${enable_nss}" = "xstatic"; then
	if test -n "${with_nspr_includes}" || test -n "${with_nspr_libs}" || test -n "${with_nss_includes}" || test -n "${with_nss_libs}" || test "x${enable_nss}" = "xstatic"; then
		check_manually="yes"
	else
		check_manually="no"
	fi

	if test "x${check_manually}" = "xno"; then
		AC_MSG_CHECKING([Mozilla NSPR pkg-config module name])
		mozilla_nspr_pcs="nspr mozilla-nspr firefox-nspr xulrunner-nspr seamonkey-nspr"
		for pc in $mozilla_nspr_pcs; do
			if $PKG_CONFIG --exists $pc; then
				AC_MSG_RESULT([$pc])
				mozilla_nspr=$pc
				break;
			fi
		done

		AC_MSG_CHECKING([Mozilla NSS pkg-config module name])
		mozilla_nss_pcs="nss mozilla-nss firefox-nss xulrunner-nss seamonkey-nss"
		for pc in $mozilla_nss_pcs; do
			if $PKG_CONFIG --exists $pc; then
				AC_MSG_RESULT([$pc])
				mozilla_nss=$pc
				break;
			fi
		done

		if test -n "$mozilla_nspr" -a -n "$mozilla_nss"; then
			msg_ssl="yes (Mozilla NSS)"
			if test "x$enable_smime" = "xyes"; then
				AC_DEFINE(ENABLE_SMIME,1,[Define if SMIME should be enabled])
				msg_smime="yes (Mozilla NSS)"
			fi
			AC_DEFINE(HAVE_NSS,1,[Define if you have NSS])
			AC_DEFINE(HAVE_SSL,1,[Define if you have a supported SSL library])
			AC_DEFINE_UNQUOTED(MOZILLA_NSS_LIB_DIR,"`$PKG_CONFIG --variable=libdir $mozilla_nss`",[Define to the full path of mozilla nss library])
			MANUAL_NSPR_CFLAGS=""
			MANUAL_NSPR_LIBS=""
			MANUAL_NSS_CFLAGS=""
			MANUAL_NSS_LIBS=""
		else
			check_manually="yes"
			mozilla_nspr=""
			mozilla_nss=""
		fi
	fi

	if test "x${check_manually}" = "xyes"; then
		mozilla_nss=""
		have_nspr_includes="no"
		if test "x${with_nspr_includes}" != "xno"; then
			CPPFLAGS_save="$CPPFLAGS"

			AC_MSG_CHECKING([for Mozilla nspr4 includes in $with_nspr_includes])
			AC_MSG_RESULT([""])

			CPPFLAGS="$CPPFLAGS -I$with_nspr_includes"
			AC_CHECK_HEADERS([nspr.h prio.h],[ moz_nspr_includes="yes" ],,
			[[
				#if HAVE_NSPR_H
				#include <nspr.h>
				#include <prio.h>
				#endif
			]])
			CPPFLAGS="$CPPFLAGS_save"

			if test "x{$moz_nspr_includes}" != "xno" -a "x{$moz_nspr_includes}" != "x" ; then
				have_nspr_includes="yes"
				MANUAL_NSPR_CFLAGS="-I$with_nspr_includes"
			fi
		else
			AC_MSG_CHECKING([for Mozilla nspr4 includes])
			AC_MSG_RESULT([no])
		fi

		have_nspr_libs="no"
		if test "x${with_nspr_libs}" != "xno" -a "x${have_nspr_includes}" != "xno"; then
			CFLAGS_save="$CFLAGS"
			LDFLAGS_save="$LDFLAGS"

			if test "$enable_nss" = "static"; then
				if test -z "${with_nspr_libs}"; then
					AC_MSG_ERROR([Static linkage requested, but path to nspr libraries not set.]
					[Please specify the path to libnspr4.a]
					[Example: --with-nspr-libs=/usr/lib])
				else
					nsprlibs="$DL_LIB $with_nspr_libs/libplc4.a $with_nspr_libs/libplds4.a $with_nspr_libs/libnspr4.a $PTHREAD_LIB"
				fi
			else
				nsprlibs="$DL_LIB -lplc4 -lplds4 -lnspr4 $PTHREAD_LIB"
			fi

			AC_CACHE_CHECK([for Mozilla nspr libraries], [ac_cv_moz_nspr_libs],
			[
		       		LIBS_save="$LIBS"
				CFLAGS="$CFLAGS $MANUAL_NSPR_CFLAGS"

				if test "x${with_nspr_libs}" != "x"; then
		       			LIBS="$nsprlibs"
		       			LDFLAGS="$LDFLAGS -L$with_nspr_libs"
				else
		       			LIBS="$nsprlibs"
		       			LDFLAGS="$LDFLAGS"
				fi

				AC_LINK_IFELSE([AC_LANG_CALL([], [PR_Init])],
				[ac_cv_moz_nspr_libs="yes"], [ac_cv_moz_nspr_libs="no"])
				CFLAGS="$CFLAGS_save"
				LDFLAGS="$LDFLAGS_save"
	 			LIBS="$LIBS_save"
			])
			if test "x$ac_cv_moz_nspr_libs" != "xno"; then
				have_nspr_libs="yes"
				MANUAL_NSPR_LIBS="-L$with_nspr_libs $nsprlibs"
			else
				MANUAL_NSPR_CLFAGS=""
			fi
		else
			AC_MSG_CHECKING([for Mozilla nspr4 libraries])
			AC_MSG_RESULT([no])
		fi

		if test "x${with_nss_includes}" != "xno" -a "x${have_nspr_libs}" != "xno"; then
			CPPFLAGS_save="$CPPFLAGS"

			AC_MSG_CHECKING([for Mozilla nss3 includes in $with_nss_includes])
			AC_MSG_RESULT([""])

			if test "x${with_nspr_includes}" != "x"; then
				CPPFLAGS="$CPPFLAGS -I$with_nspr_includes -I$with_nss_includes"
			else
				CPPFLAGS="$CPPFLAGS -I$with_nss_includes"
			fi

			AC_CHECK_HEADERS(nss.h ssl.h smime.h,
				[ have_nss_includes="yes" ],
				[ have_nss_includes="no" ],
				[
					#if HAVE_NSPR_H
					#include <nss.h>
					#include <ssl.h>
					#include <smime.h>
					#endif
				])

			CPPFLAGS="$CPPFLAGS_save"

			if test "x${have_nss_includes}" = "xyes"; then
				have_nss_includes="yes"
				MANUAL_NSS_CFLAGS="-I$with_nss_includes"
			else
				MANUAL_NSPR_CFLAGS=""
				MANUAL_NSPR_LIBS=""
			fi
		else
			AC_MSG_CHECKING([for Mozilla nss3 includes])
			AC_MSG_RESULT([no])
		fi

		if test "x${with_nss_libs}" != "xno" -a "x${have_nss_includes}" != "xno"; then
			LDFLAGS_save="$LDFLAGS"

			if test "$enable_nss" = "static"; then
		    		if test -z "${with_nss_libs}"; then
					AC_MSG_ERROR([Static linkage requested, but path to nss libraries not set.]
					[Please specify the path to libnss3.a]
					[Example: --with-nspr-libs=/usr/lib/mozilla])
				else
					nsslibs="-ldb1 $with_nss_libs/libnssckfw.a $with_nss_libs/libasn1.a $with_nss_libs/libcrmf.a $with_nss_libs/libswfci.a $with_nss_libs/libjar.a $with_nss_libs/libpkcs12.a $with_nss_libs/libpkcs7.a $with_nss_libs/libpki1.a $with_nss_libs/libsmime.a $with_nss_libs/libssl.a $with_nss_libs/libnss.a $with_nss_libs/libpk11wrap.a $with_nss_libs/libsoftokn.a $with_nss_libs/libfreebl.a $with_nss_libs/libnsspki.a $with_nss_libs/libnssdev.a $with_nss_libs/libcryptohi.a $with_nss_libs/libcerthi.a $with_nss_libs/libcertdb.a $with_nss_libs/libsecutil.a $with_nss_libs/libnssb.a"
					case "$host" in
						*solaris*)
						nsslibs="$nsslibs $with_nss_libs/libfreebl.a"
					;;
					esac
				fi
			else
		 		nsslibs="-lssl3 -lsmime3 -lnss3"
			fi

			AC_CACHE_CHECK([for Mozilla nss libraries], [ac_cv_moz_nss_libs],
			[
	 			LIBS_save="$LIBS"
				LDFLAGS="$LDFLAGS -L$with_nspr_libs $nsprlibs -L$with_nss_libs $nsslibs"
		      		LIBS="$nsslibs $nsprlibs"
				AC_LINK_IFELSE([AC_LANG_CALL([], [NSS_Init])],
				[ac_cv_moz_nss_libs="yes"], [ac_cv_moz_nss_libs="no"])
				if test "$ac_cv_moz_nss_libs" = no; then
					nsslibs="-lssl3 -lsmime3 -lnss3"
					LDFLAGS="$LDFLAGS -L$with_nspr_libs $nsprlibs -L$with_nss_libs $nsslibs"
					AC_LINK_IFELSE([AC_LANG_CALL([], [NSS_Init])],
					[ac_cv_moz_nss_libs="yes"], [ac_cv_moz_nss_libs="no"])
				fi
				LDFLAGS="$LDFLAGS_save"
	 			LIBS="$LIBS_save"
			])
			if test "$ac_cv_moz_nss_libs" != no; then
				AC_DEFINE(HAVE_NSS)
				AC_DEFINE(HAVE_SSL)
				AC_DEFINE_UNQUOTED(MOZILLA_NSS_LIB_DIR,"$with_nss_libs", [Define to the full path of mozilla nss library])
				if test "$enable_nss" = "static"; then
					msg_ssl="yes (Mozilla NSS:static)"
		 		else
					msg_ssl="yes (Mozilla NSS)"
				fi
				dnl static_nss
				if test "$enable_smime" = "yes"; then
					AC_DEFINE(ENABLE_SMIME,1,[Define if SMIME should be enabled])
					msg_smime="yes (Mozilla NSS)"
				fi
				MANUAL_NSS_LIBS="-L$with_nss_libs $nsslibs"
			else
				MANUAL_NSS_CFLAGS=""
				MANUAL_NSPR_CFLAGS=""
				MANUAL_NSPR_LIBS=""
			fi
		else
			AC_MSG_CHECKING([for Mozilla nss libraries])
			AC_MSG_RESULT([no])
		fi

		MANUAL_NSS_CFLAGS="$MANUAL_NSPR_CFLAGS $MANUAL_NSS_CFLAGS"
		MANUAL_NSS_LIBS="$MANUAL_NSPR_LIBS $MANUAL_NSS_LIBS"
	fi
fi

AM_CONDITIONAL(ENABLE_SMIME, [test "x$msg_smime" != "xno"])

AC_SUBST(MANUAL_NSPR_CFLAGS)
AC_SUBST(MANUAL_NSPR_LIBS)
AC_SUBST(MANUAL_NSS_CFLAGS)
AC_SUBST(MANUAL_NSS_LIBS)

dnl **************************************************
dnl Check if we should build the weather calendar plugin
dnl **************************************************
AC_MSG_CHECKING([if we should build the weather calendar plugin])
AC_ARG_WITH([weather],
	[AS_HELP_STRING([--with-weather],
	[Build the weather calendar setup plugin (default=yes)])],
	[use_gweather=$withval],[use_gweather=yes])
AC_MSG_RESULT([$use_gweather])

if test $use_gweather = yes; then
	PKG_CHECK_MODULES([LIBGWEATHER],
	[gweather >= gweather_minimum_version],
	[CALENDAR_WEATHER="calendar-weather"],
	[AC_MSG_ERROR([The weather calendar setup plugin requires GWeather >= gweather_minimum_version. Alternatively, you may specify --without-weather as a configure option to avoid building the plugin.])])
else
	CALENDAR_WEATHER=""
fi

dnl **************************************************
dnl Exchange support.
dnl **************************************************
AC_ARG_ENABLE([exchange],
	AS_HELP_STRING([--enable-exchange],
	[Build Exchange plugins]),
	[enable_exchange=$enableval],[enable_exchange=yes])

if test "x$enable_exchange" = "xyes"; then
	msg_exchange=yes
	EXCHANGE_PLUGIN="exchange-operations"
else
	msg_exchange=no
fi
AM_CONDITIONAL(ENABLE_EXCHANGE, [test x$enable_exchange = xyes])

dnl *****************
dnl killall or pkill?
dnl *****************

case "$host" in
*solaris*)
	KILL_PROCESS_CMD="/usr/bin/pkill -x"
	;;
*)
	AC_PATH_PROGS(KILL_PROCESS_CMD, killall pkill)
	;;
esac
if test -z "$KILL_PROCESS_CMD"; then
	AC_MSG_WARN([Could not find a command to kill a process by name])
else
	AC_DEFINE_UNQUOTED([KILL_PROCESS_CMD], "$KILL_PROCESS_CMD",
		[Command to kill processes by name])
fi

dnl *******************
dnl GObject marshalling
dnl *******************
AM_PATH_GLIB_2_0

dnl We use AC_SUBST_FILE because AC_SUBST won't deal with newlines
EVO_MARSHAL_RULE=$srcdir/marshal.mk
AC_SUBST_FILE(EVO_MARSHAL_RULE)

dnl **************************
dnl Check for X libs and -lX11
dnl **************************

PKG_CHECK_MODULES([X], [x11], [:], [
dnl pkg-config modules not found (only present since X11R7 aka Xorg); use
dnl old-style detection
AC_PATH_XTRA
dnl X found
if test x$no_x != xyes ; then
	evolution_save_cflags="$CFLAGS"
	evolution_save_libs="$LIBS"
	CFLAGS="$X_CFLAGS"
	LIBS="$X_PRE_LIBS $X_LIBS $X_EXTRA_LIBS"
	AC_CHECK_LIB(X11, XFree, :, AC_MSG_ERROR([libX11 not found]))
	CFLAGS="$evolution_save_cflags"
	LIBS="$evolution_save_libs"
	X_LIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
fi
])

dnl *************************
dnl Check for the Gtk backend
dnl if it's X11, we -lX11
dnl *************************

evolution_save_cflags="$CFLAGS"
dnl pull in the CFLAGS to locate gdkconfig.h
CFLAGS=`$PKG_CONFIG --cflags gtk+-2.0`
AC_COMPILE_IFELSE([
	#include <gdkconfig.h>
	int main(void){
	#ifndef GDK_WINDOWING_X11
	#error GDK_WINDOWING_X11 not defined
	#endif
	return 0;
	}],
	GDK_BACKEND_X11=yes,
	GDK_BACKEND_X11=no)

CFLAGS="$evolution_save_cflags"


AM_PATH_ORBIT2(2.9.8)

AC_MSG_CHECKING([for CORBA include paths])
IDL_INCLUDES="-I "`${PKG_CONFIG} --variable=idldir libbonobo-2.0`" -I "`${PKG_CONFIG} --variable=idldir bonobo-activation-2.0`" -I "`${PKG_CONFIG} --variable=idldir evolution-data-server-1.2`
AC_MSG_RESULT([$IDL_INCLUDES])
AC_SUBST(IDL_INCLUDES)

dnl Utility macro to set compiler flags for a specific lib.
dnl EVO_SET_COMPILE_FLAGS(VAR-PREFIX, DEPS, EXTRA-CFLAGS, EXTRA-LIBS)
AC_DEFUN([EVO_SET_COMPILE_FLAGS], [
	PKG_CHECK_MODULES([$1], [$2])
	$1_CFLAGS="[$]$1_CFLAGS \$(WERROR) $3"
	$1_LIBS="[$]$1_LIBS $4"
])

dnl --- Required version numbers

BONOBOUI_REQUIRED=libbonoboui_minimum_version
AC_SUBST(BONOBOUI_REQUIRED)

dnl --- Flags to get all the GNOME stuff

FULL_GNOME_DEPS="libbonoboui-2.0 gconf-2.0 gthread-2.0 gobject-2.0 libgnomeui-2.0 libglade-2.0"

PKG_CHECK_MODULES([HAL], [hal >= hal_minimum_version], [HAVE_HAL="yes"], [HAVE_HAL="no"])
if test "x$HAVE_HAL" = "xyes"; then
	AC_DEFINE(HAVE_HAL, 1, [hal available])
	HAL_REQUIREMENT="hal"
	IPOD_SYNC="ipod-sync"
else
	HAL_REQUIREMENT=""
	IPOD_SYNC=""
fi

AC_MSG_CHECKING([for yTNEF])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[
		#include <stdio.h>
		#include <ytnef.h>
	]],
	[[	TNEFStruct *tnef;	]]
	)],	[tnef_ok=yes], [tnef_ok=no])
if test "$tnef_ok" = "yes"; then
	AC_MSG_RESULT([yes])
	TNEF_ATTACHMENTS="tnef-attachments"
	TNEF_CFLAGS="-DHAVE_YTNEF_H"
else
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[
		#include <stdio.h>
		#include <libytnef/ytnef.h>
	]],
	[[	TNEFStruct *tnef;	]]
	)], [tnef_ok=yes], [tnef_ok=no])
	if test "$tnef_ok" = "yes"; then
		AC_MSG_RESULT([yes])
		TNEF_ATTACHMENTS="tnef-attachments"
		TNEF_CFLAGS="-DHAVE_LIBYTNEF_YTNEF_H"
	else
		AC_MSG_RESULT([no])
		TNEF_ATTACHMENTS=""
		TNEF_CFLAGS=""
	fi
fi
AC_SUBST(TNEF_CFLAGS)

dnl Check for dbus-glib-1 and dbus
AC_ARG_ENABLE([dbus],
	[AS_HELP_STRING([--enable-dbus],
	[enable DBUS support (default=yes)])],
	[enable_dbus=$enableval],[enable_dbus=yes])
AC_MSG_CHECKING([if dbus support is enabled])
AC_MSG_RESULT([$enable_dbus])
if test "$enable_dbus" = "yes"; then
	PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1 >= dbus_minimum_version],
	[DBUS_SUPPORT="yes"],
	[AC_MSG_ERROR([DBUS respectively dbus-glib not found! (or version < dbus_minimum_version)!
	If you want to disable DBUS, please append --disable-dbus to configure!
	DBUS is required for NetworkManager - NetworkManager support will then be disabled, too!])])
	AC_DEFINE(DBUS_SUPPORT, 1, [dbus available])
	AC_SUBST(HAVE_DBUS)
	AC_SUBST(DBUS_GLIB_CFLAGS)
	AC_SUBST(DBUS_GLIB_LIBS)
elif test "$enable_dbus" = "no" -a "$enable_nm" = "yes"; then
	AC_MSG_ERROR([DBUS disabled and NetworkManager enabled - this won't work!
	Disable NetworkManager (append --disable-nm to configure)!])
else
	AC_MSG_WARN([DBUS support disabled!])
	DBUS_SUPPORT="no"
fi
AM_CONDITIONAL([DBUS_SUPPORT], [test "$DBUS_SUPPORT" = yes])

dnl Check for NetworkManager
AC_ARG_ENABLE([nm],
	[AS_HELP_STRING([--enable-nm],
	[enable NetworkManager support (default=yes)])],
	[enable_nm=$enableval],[enable_nm=yes])
AC_MSG_CHECKING([if NetworkManager support is enabled])
AC_MSG_RESULT([$enable_nm])
if test "$enable_nm" = yes; then
	PKG_CHECK_MODULES([NM], [NetworkManager >= nm_minimum_version],
	[NM_SUPPORT="yes"],
	[AC_MSG_ERROR([NetworkManager not found (or version < nm_minimum_version)!
	If you want to disable NetworkManager, please append --disable-nm to configure!])])
	AC_DEFINE(NM_SUPPORT, 1, [network manager available])
	AC_SUBST(HAVE_NM)
	AC_SUBST(NM_CFLAGS)
else
	NM_SUPPORT="no"
fi

AM_CONDITIONAL([NM_SUPPORT], [test "$NM_SUPPORT" = yes])

dnl --- Flags for the various libraries we build

EVO_SET_COMPILE_FLAGS(CAMEL, camel-provider-$EDS_PACKAGE)
AC_SUBST(CAMEL_CFLAGS)
AC_SUBST(CAMEL_LIBS)

EVO_SET_COMPILE_FLAGS(CAMEL_GROUPWISE, camel-provider-$EDS_PACKAGE libedataserver-$EDS_PACKAGE libegroupwise-$EDS_PACKAGE)
AC_SUBST(CAMEL_GROUPWISE_CFLAGS)
AC_SUBST(CAMEL_GROUPWISE_LIBS)

if test "x$mozilla_nss" != "x"; then
	EVO_SET_COMPILE_FLAGS(MOZILLA_NSS, $mozilla_nss)
	MOZILLA_L_DIR=`${PKG_CONFIG} --libs-only-L $mozilla_nss`
else
	if test "x$MOZILLA_MANUAL_LIBS" != "x"; then
		MOZILLA_L_DIR="-L$with_nss_lib"
	fi
fi

EVO_SET_COMPILE_FLAGS(CERT_UI, libedataserverui-$EDS_PACKAGE libglade-2.0 gthread-2.0, $MANUAL_NSS_CFLAGS $MOZILLA_NSS_CFLAGS, 	$MOZILLA_L_DIR $MANUAL_NSS_LIBS $MOZILLA_NSS_LIBS)
AC_SUBST(CERT_UI_CFLAGS)
AC_SUBST(CERT_UI_LIBS)

EVO_SET_COMPILE_FLAGS(E_NAME, libgnomeui-2.0 libbonoboui-2.0)
AC_SUBST(E_NAME_CFLAGS)
AC_SUBST(E_NAME_LIBS)

dnl If Gtk/Gdk is built against X11, e-util uses some
dnl X11-specific code so link with X11.
if test "x$GDK_BACKEND_X11" = "xyes"; then
	E_UTIL_X11_CFLAGS="$X_CFLAGS"
	E_UTIL_X11_LIBS="$X_LIBS"
else
	E_UTIL_X11_CFLAGS=""
	E_UTIL_X11_LIBS=""
fi
EVO_SET_COMPILE_FLAGS(E_UTIL, libbonoboui-2.0 libglade-2.0 libgnomeui-2.0 gnome-desktop-2.0 libedataserver-$EDS_PACKAGE libedataserverui-$EDS_PACKAGE camel-$EDS_PACKAGE $mozilla_nspr, $THREADS_CFLAGS $MANUAL_NSPR_CFLAGS $E_UTIL_X11_CFLAGS, $THREADS_LIBS $MANUAL_NSPR_LIBS $E_UTIL_X11_LIBS)
AC_SUBST(E_UTIL_CFLAGS)
AC_SUBST(E_UTIL_LIBS)

EVO_SET_COMPILE_FLAGS(TZDIALOG, libecal-$EDS_PACKAGE, $GNOME_PLATFORM_CFLAGS, $GNOME_PLATFORM_LIBS)
AC_SUBST(TZDIALOG_CFLAGS)
AC_SUBST(TZDIALOG_LIBS)

EVO_SET_COMPILE_FLAGS(E_WIDGETS, libbonoboui-2.0 gio-2.0 gconf-2.0 gobject-2.0 libgnomeui-2.0 libglade-2.0 libedataserverui-$EDS_PACKAGE libedataserver-$EDS_PACKAGE)
AC_SUBST(E_WIDGETS_CFLAGS)
AC_SUBST(E_WIDGETS_LIBS)

EVO_SET_COMPILE_FLAGS(IMPORTERS, libedataserverui-$EDS_PACKAGE camel-provider-$EDS_PACKAGE camel-$EDS_PACKAGE libglade-2.0 libbonoboui-2.0 libgnomeui-2.0 libebook-$EDS_PACKAGE)
AC_SUBST(IMPORTERS_CFLAGS)
AC_SUBST(IMPORTERS_LIBS)

EVO_SET_COMPILE_FLAGS(LIBFILTER, libgnomeui-2.0 libglade-2.0 libedataserver-$EDS_PACKAGE)
AC_SUBST(LIBFILTER_CFLAGS)
AC_SUBST(LIBFILTER_LIBS)

dnl --- evolution (shell) flags
EVO_SET_COMPILE_FLAGS(SHELL, libgnomeui-2.0 libbonoboui-2.0 libglade-2.0 gio-2.0 gconf-2.0 gobject-2.0 libgtkhtml-$GTKHTML_PACKAGE)

AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

dnl --- evolution-addressbook flags

EVOLUTION_ADDRESSBOOK_DEPS="libbonoboui-2.0 libglade-2.0 libgnomeui-2.0 gio-2.0 gconf-2.0 gobject-2.0 libgtkhtml-$GTKHTML_PACKAGE libebook-$EDS_PACKAGE libedataserverui-$EDS_PACKAGE camel-$EDS_PACKAGE gtkhtml-editor"

EVO_SET_COMPILE_FLAGS(EVOLUTION_ADDRESSBOOK, $EVOLUTION_ADDRESSBOOK_DEPS)
AC_SUBST(EVOLUTION_ADDRESSBOOK_CFLAGS)
AC_SUBST(EVOLUTION_ADDRESSBOOK_LIBS)

if test x$enable_pilot_conduits = xyes; then
EVO_SET_COMPILE_FLAGS(EVOLUTION_ADDRESSBOOK_CONDUIT, gnome-pilot-2.0 $EVOLUTION_ADDRESSBOOK_DEPS)
AC_SUBST(EVOLUTION_ADDRESSBOOK_CONDUIT_CFLAGS)
AC_SUBST(EVOLUTION_ADDRESSBOOK_CONDUIT_LIBS)
fi

LIBNOTIFY_CFLAGS=
LIBNOTIFY_LIBS=
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= libnotify_minimum_version], [HAVE_LIBNOTIFY="yes"], [HAVE_LIBNOTIFY="no"])
if test "x$HAVE_LIBNOTIFY" = "xyes"; then
	AC_DEFINE(HAVE_LIBNOTIFY, 1, [libnotify available])
	libnotify="libnotify"
else
	libnotify=""
fi
AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)

dnl --- evolution-calendar flags
EVO_SET_COMPILE_FLAGS(LIBSOUP, libsoup-2.4 >= 2.3.0)
AC_SUBST(LIBSOUP_CFLAGS)
AC_SUBST(LIBSOUP_LIBS)

EVO_SET_COMPILE_FLAGS(EVOLUTION_CALENDAR, libgnomeui-2.0 libbonoboui-2.0 libglade-2.0 gio-2.0 gconf-2.0 gobject-2.0 libgtkhtml-$GTKHTML_PACKAGE libebook-$EDS_PACKAGE libecal-$EDS_PACKAGE libedataserverui-$EDS_PACKAGE libebackend-$EDS_PACKAGE $HAL_REQUIREMENT $libnotify gtkhtml-editor libgdata-$EDS_PACKAGE libgdata-google-$EDS_PACKAGE)
AC_SUBST(EVOLUTION_CALENDAR_CFLAGS)
AC_SUBST(EVOLUTION_CALENDAR_LIBS)

if test x$enable_pilot_conduits = xyes; then
EVO_SET_COMPILE_FLAGS(EVOLUTION_CALENDAR_CONDUIT, gnome-pilot-2.0, $EVOLUTION_CALENDAR_CFLAGS, $EVOLUTION_CALENDAR_LIBS)
AC_SUBST(EVOLUTION_CALENDAR_CONDUIT_CFLAGS)
AC_SUBST(EVOLUTION_CALENDAR_CONDUIT_LIBS)
fi

dnl --- evolution-mail flags

EVO_SET_COMPILE_FLAGS(EVOLUTION_MAIL, camel-provider-$EDS_PACKAGE libgnomeui-2.0 libbonoboui-2.0 libglade-2.0 gio-2.0 gconf-2.0 gobject-2.0 libgtkhtml-$GTKHTML_PACKAGE gtkhtml-editor bonobo-activation-2.0 $mozilla_nss libebook-$EDS_PACKAGE libedataserverui-$EDS_PACKAGE)
AC_SUBST(EVOLUTION_MAIL_CFLAGS)
AC_SUBST(EVOLUTION_MAIL_LIBS)

dnl -- evolution-data-server IDL and version

AC_DEFINE(DATASERVER_API_VERSION, "1.2", evolution-data-server API version)
AC_DEFINE_UNQUOTED(DATASERVER_VERSION, "`${PKG_CONFIG} --modversion evolution-data-server-1.2`", evolution-data-server version)

DATASERVER_EXEC_VERSION=`${PKG_CONFIG} --variable=execversion evolution-data-server-1.2`
AC_SUBST(DATASERVER_EXEC_VERSION)

AC_MSG_CHECKING([for evolution-data-server IDL])
DATASERVER_IDL=`${PKG_CONFIG} --variable=idldir evolution-data-server-1.2`/Evolution-DataServer.idl
if test -f "$DATASERVER_IDL"; then
	AC_MSG_RESULT([$DATASERVER_IDL])
	AC_SUBST(DATASERVER_IDL)
else
	AC_MSG_ERROR([no])
fi

dnl --- evolution-test flags

EVO_SET_COMPILE_FLAGS(EVOLUTION_TEST, libgnomeui-2.0 libbonoboui-2.0)
AC_SUBST(EVOLUTION_TEST_CFLAGS)
AC_SUBST(EVOLUTION_TEST_LIBS)

dnl *******************
dnl Special directories
dnl *******************

dnl --- If you add something here, consider whether or not you also
dnl --- need to add it to one or more .pc.in files (for Connector,
dnl --- etc)

privlibdir='${libdir}'/evolution/$BASE_VERSION
AC_SUBST(privlibdir)

privlibexecdir='${libexecdir}'/evolution/$BASE_VERSION
AC_SUBST(privlibexecdir)

privdatadir='${datadir}'/evolution/$BASE_VERSION
AC_SUBST(privdatadir)

privincludedir='${includedir}'/evolution-$BASE_VERSION
AC_SUBST(privincludedir)

componentdir="$privlibdir/components"
AC_SUBST(componentdir)

idldir='${datadir}'/idl/evolution-$BASE_VERSION
AC_SUBST(idldir)

serverdir="$libdir/bonobo/servers"
AC_SUBST(serverdir)

evolutionuidir="$privdatadir/ui"
AC_SUBST(evolutionuidir)

evolutionhelpdir="$privdatadir/help"
AC_SUBST(evolutionhelpdir)

imagesdir="$privdatadir/images"
AC_SUBST(imagesdir)

images16dir="$privdatadir/images/16x16"
AC_SUBST(images16dir)

if test "$os_win32" = yes; then
	dnl On Win32 there is no "rpath" mechanism. We install the private
	dnl shared libraries in $libdir, meaning the DLLs will actually be in
	dnl $bindir. This means just having $bindir in PATH will be enough. This
	dnl also means gnome_win32_get_prefixes() will be able to deduce the
	dnl installation folder correctly.
	privsolibdir=$libdir
	dnl On Win32, use a relative path to the executables in .server
	dnl files. The path is relative to the folder where the .server file
	dnl is, i.e. $prefix/lib/bonobo/servers.
	bindir_in_server_file=../../../bin
	privlibexecdir_in_server_file=../../../libexec/evolution/$BASE_VERSION
	componentdir_in_server_file=../../../lib/evolution/$BASE_VERSION/components
else
	privsolibdir=$privlibdir
	bindir_in_server_file="$bindir"
	privlibexecdir_in_server_file='${libexecdir}'/evolution/$BASE_VERSION
	componentdir_in_server_file="$componentdir"
fi
AC_SUBST(privsolibdir)
AC_SUBST(bindir_in_server_file)
AC_SUBST(privlibexecdir_in_server_file)
AC_SUBST(componentdir_in_server_file)

dnl images18dir="$privdatadir/images/18x18"
dnl AC_SUBST(images18dir)

dnl images20dir="$privdatadir/images/20x20"
dnl AC_SUBST(images20dir)

dnl images24dir="$privdatadir/images/24x24"
dnl AC_SUBST(images24dir)

dnl images32dir="$privdatadir/images/32x32"
dnl AC_SUBST(images32dir)

images48dir="$privdatadir/images/48x48"
AC_SUBST(images48dir)

soundsdir="$privdatadir/sounds"
AC_SUBST(soundsdir)

gladedir="$privdatadir/glade"
AC_SUBST(gladedir)

etspecdir="$privdatadir/etspec"
AC_SUBST(etspecdir)

viewsdir="$privdatadir/views"
AC_SUBST(viewsdir)

privconduitdir='${libdir}'/evolution/$BASE_VERSION/conduits
AC_SUBST(privconduitdir)

dnl ************************
dnl IDL/Component Versioning
dnl ************************

INTERFACE_VERSION="$BASE_VERSION"
AC_SUBST(INTERFACE_VERSION)
AC_DEFINE_UNQUOTED(INTERFACE_VERSION, "$INTERFACE_VERSION", [IDL interface version (Major.Minor)])

EVO_SERVER_RULE=$srcdir/server.mk
AC_SUBST_FILE(EVO_SERVER_RULE)

dnl ************************
dnl Plugins
dnl ************************

plugindir="$privlibdir/plugins"
AC_SUBST(plugindir)

EVO_PLUGIN_RULE=$srcdir/plugin.mk
AC_SUBST_FILE(EVO_PLUGIN_RULE)

AC_ARG_ENABLE([plugins],
	AS_HELP_STRING([--enable-plugins=[no/base/all/experimental/list]],
	[Enable plugins.]),
	[enable_plugins="$enableval"],[enable_plugins=all])

dnl Add any new plugins here
plugins_base_always="calendar-file calendar-http $CALENDAR_WEATHER itip-formatter plugin-manager default-source addressbook-file startup-wizard mark-all-read groupwise-features groupwise-account-setup mail-account-disable publish-calendar caldav imap-features google-account-setup webdav-account-setup"

plugins_base="$plugins_base_always $SA_JUNK_PLUGIN $BF_JUNK_PLUGIN $EXCHANGE_PLUGIN $MONO_PLUGIN "
all_plugins_base="$plugins_base_always sa-junk-plugin bogo-junk-plugin exchange-operations mono"

plugins_standard_always="bbdb subject-thread save-calendar select-one-source copy-tool mail-to-task audio-inline mailing-list-actions default-mailer prefer-plain mail-notification attachment-reminder backup-restore email-custom-header templates pst-import vcard-inline"

plugins_standard="$plugins_standard_always"
all_plugins_standard="$plugins_standard"

plugins_experimental_always="face folder-unsubscribe external-editor hula-account-setup"
plugins_experimental="$plugins_experimental_always $IPOD_SYNC $TNEF_ATTACHMENTS $PYTHON_PLUGIN"
all_plugins_experimental="$plugins_experimental_always ipod-sync tnef-attachments"

case x"$enable_plugins" in
xno)
	plugins_enabled=""
	msg_plugins="no (some core functionality will not be available)"
	;;
xall | x | xyes)
	plugins_enabled="$plugins_base $plugins_standard"
	msg_plugins="yes (all)"
	;;
xbase)
	plugins_enabled="$plugins_base"
	msg_plugins="yes ($plugins_base)"
	;;
xexperimental)
	plugins_enabled="$plugins_base $plugins_standard $plugins_experimental"
	msg_plugins="yes ($plugins_base $plugins_standard $plugins_experimental)"
	;;
*)
	plugins_enabled="$enable_plugins"
	msg_plugins="yes ($enable_plugins)"
	;;
esac

AC_ARG_ENABLE([profiling],
	AS_HELP_STRING([--enable-profiling],
	[Enable profiling plugin.]),
	[enable_profiling=$enableval],[enable_profiling=no])

case x"$enable_profiling" in
x | xyes)
	plugins_enabled="$plugins_enabled profiler"
	msg_plugins="$msg_plugins (and profiling)"
	AC_DEFINE(ENABLE_PROFILING,1,[Profiling Hooks Enabled])
	;;
esac

if test "x${enable_mono}" = "xyes"; then
	plugins_enabled="$plugins_enabled mono"
	msg_plugins="$msg_plugins (and mono)"
fi


if test "x${enable_python}" = "xyes"; then
	plugins_enabled="$plugins_enabled python"
	msg_plugins="$msg_plugins (and python)"
fi

AC_SUBST(plugins_enabled)
AC_SUBST(all_plugins_base)
AC_SUBST(all_plugins_standard)
AC_SUBST(all_plugins_experimental)

if echo ${plugins_enabled} | grep "audio-inline" > /dev/null
then
	if ${PKG_CONFIG} --exists gstreamer-0.10
	then
		dnl *********************
		dnl gstreamer
		dnl *********************
		PKG_CHECK_MODULES([GSTREAMER], [gstreamer-0.10])
		AC_SUBST(GSTREAMER_CFLAGS)
		AC_SUBST(GSTREAMER_LIBS)
	else
		plugins_enabled=`echo $plugins_enabled | sed -e "s/audio-inline//g"`
		echo "warning: gstreamer was not found, audio-inline plugin will not be built."
		echo "you are probably missing gstreamer-devel package."
	fi
fi

if echo ${plugins_enabled} | grep "exchange-operations" > /dev/null ; then
	PKG_CHECK_MODULES([LIBEXCHANGESTORAGE], [libexchange-storage-$EDS_PACKAGE >= eds_minimum_version], [have_libexchange="yes"], [have_libexchange="no"])
	if test "x$have_libexchange" = "xyes"; then
		dnl **************************************************
		dnl * Exchange Operations plugin
		dnl **************************************************
		EVO_SET_COMPILE_FLAGS(CAMEL_EXCHANGE, libbonoboui-2.0 libglade-2.0 gconf-2.0 camel-provider-$EDS_PACKAGE libebook-$EDS_PACKAGE libedataserverui-$EDS_PACKAGE libexchange-storage-$EDS_PACKAGE libecal-$EDS_PACKAGE)
		AC_SUBST(CAMEL_EXCHANGE_CFLAGS)
		AC_SUBST(CAMEL_EXCHANGE_LIBS)
	else
		plugins_enabled=`echo $plugins_enabled | sed -e "s/exchange-operations//g"`
		echo "warning: libevolution-exchange-$EDS_PACKAGE was not found, Exchange Operations plugin will not be built."
	fi
fi

if echo ${plugins_enabled} | grep "pst-import" > /dev/null
then
	if ${PKG_CONFIG} --exists libpst
	then
		dnl *********************
		dnl libpst
		dnl *********************
		PKG_CHECK_MODULES([LIBPST], [libpst >= libpst_minimum_version])
		AC_SUBST(LIBPST_CFLAGS)
		AC_SUBST(LIBPST_LIBS)
	else
		plugins_enabled=`echo $plugins_enabled | sed -e "s/pst-import//g"`
		echo "warning: libpst was not found, pst-import plugin will not be built."
		echo "you are probably missing libpst-devel package."
	fi
fi

dnl ***********
dnl GConf stuff
dnl ***********
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AM_GCONF_SOURCE_2

dnl ******************
dnl Sub-version number
dnl ******************
AC_ARG_WITH([sub-version],
	AS_HELP_STRING([--with-sub-version=VERSION],
	[Specify a sub-version string]))

AC_DEFINE_UNQUOTED(SUB_VERSION, "$with_sub_version", [Version substring, for packagers])

AC_ARG_ENABLE([default-binary],
	AS_HELP_STRING([--disable-default-binary],
	[Do not install as the default "evolution" binary]),,
	[enable_default_binary=no])

AM_CONDITIONAL(DEFAULT_BINARY, test x$enable_default_binary = xyes)

dnl ********************
dnl KDE applnk directory
dnl ********************
AC_ARG_WITH([kde-applnk-path],
	AS_HELP_STRING([--with-kde-applnk-path=PATH],
	[Location of KDE applnk files]),
	[with_kde_applnk_path=$withval], [with_kde_applnk_path="no"])

if test x"$with_kde_applnk_path" != x"no"; then
	if test -z "$with_kde_applnk_path"; then
		with_kde_applnk_path="$datadir/applnk"
	fi
	KDE_APPLNK_DIR="$with_kde_applnk_path"
else
	KDE_APPLNK_DIR=""
fi
AM_CONDITIONAL(HAVE_KDE_APPLNK, test x"$KDE_APPLNK_DIR" != x)
AC_SUBST(KDE_APPLNK_DIR)

dnl ******************************
dnl Makefiles
dnl ******************************

export privlibdir
export privincludedir
export privdatadir
export plugindir

EVOLUTION_DIR=`(cd $srcdir; pwd)`
AC_SUBST(EVOLUTION_DIR)

AC_CONFIG_FILES([ po/Makefile.in
Makefile
win32/Makefile
a11y/Makefile
a11y/addressbook/Makefile
a11y/calendar/Makefile
a11y/widgets/Makefile
a11y/e-table/Makefile
a11y/e-text/Makefile
addressbook/Makefile
addressbook/conduit/Makefile
addressbook/gui/Makefile
addressbook/gui/component/Makefile
addressbook/gui/contact-editor/Makefile
addressbook/gui/contact-list-editor/Makefile
addressbook/gui/merging/Makefile
addressbook/gui/widgets/Makefile
addressbook/importers/Makefile
addressbook/printing/Makefile
addressbook/tools/Makefile
addressbook/tools/csv2vcard
addressbook/util/Makefile
art/Makefile
data/Makefile
data/icons/Makefile
e-util/Makefile
filter/Makefile
help/Makefile
help/quickref/Makefile
help/quickref/C/Makefile
help/quickref/ca/Makefile
help/quickref/cs/Makefile
help/quickref/de/Makefile
help/quickref/es/Makefile
help/quickref/hu/Makefile
help/quickref/it/Makefile
help/quickref/fr/Makefile
help/quickref/oc/Makefile
help/quickref/pl/Makefile
help/quickref/pt/Makefile
help/quickref/sv/Makefile
help/quickref/sq/Makefile
shell/Makefile
shell/evolution-nognome
shell/test/Makefile
ui/Makefile
views/Makefile
views/addressbook/Makefile
views/calendar/Makefile
views/mail/Makefile
views/tasks/Makefile
views/memos/Makefile
widgets/Makefile
widgets/e-timezone-dialog/Makefile
widgets/menus/Makefile
widgets/misc/Makefile
widgets/text/Makefile
widgets/table/Makefile
calendar/Makefile
calendar/importers/Makefile
calendar/common/Makefile
calendar/idl/Makefile
calendar/conduits/Makefile
calendar/conduits/common/Makefile
calendar/conduits/todo/Makefile
calendar/conduits/calendar/Makefile
calendar/conduits/memo/Makefile
calendar/gui/Makefile
calendar/gui/alarm-notify/Makefile
calendar/gui/dialogs/Makefile
composer/Makefile
mail/Makefile
mail/default/Makefile
mail/default/C/Makefile
mail/default/ca/Makefile
mail/default/cs/Makefile
mail/default/de/Makefile
mail/default/es/Makefile
mail/default/fi/Makefile
mail/default/fr/Makefile
mail/default/hu/Makefile
mail/default/it/Makefile
mail/default/ja/Makefile
mail/default/ko/Makefile
mail/default/lt/Makefile
mail/default/mk/Makefile
mail/default/nl/Makefile
mail/default/pl/Makefile
mail/default/pt/Makefile
mail/default/ro/Makefile
mail/default/sr/Makefile
mail/default/sr@latin/Makefile
mail/default/sv/Makefile
mail/default/zh_CN/Makefile
mail/importers/Makefile
plugins/Makefile
plugins/addressbook-file/Makefile
plugins/attachment-reminder/Makefile
plugins/audio-inline/Makefile
plugins/backup-restore/Makefile
plugins/bbdb/Makefile
plugins/bogo-junk-plugin/Makefile
plugins/caldav/Makefile
plugins/calendar-file/Makefile
plugins/calendar-http/Makefile
plugins/calendar-weather/Makefile
plugins/copy-tool/Makefile
plugins/default-mailer/Makefile
plugins/default-source/Makefile
plugins/email-custom-header/Makefile
plugins/exchange-operations/Makefile
plugins/external-editor/Makefile
plugins/face/Makefile
plugins/folder-unsubscribe/Makefile
plugins/google-account-setup/Makefile
plugins/groupwise-account-setup/Makefile
plugins/groupwise-features/Makefile
plugins/hula-account-setup/Makefile
plugins/imap-features/Makefile
plugins/ipod-sync/Makefile
plugins/itip-formatter/Makefile
plugins/mail-account-disable/Makefile
plugins/mail-notification/Makefile
plugins/mail-to-task/Makefile
plugins/mailing-list-actions/Makefile
plugins/mark-all-read/Makefile
plugins/mono/Makefile
plugins/plugin-manager/Makefile
plugins/prefer-plain/Makefile
plugins/profiler/Makefile
plugins/pst-import/Makefile
plugins/publish-calendar/Makefile
plugins/python/Makefile
plugins/sa-junk-plugin/Makefile
plugins/save-calendar/Makefile
plugins/select-one-source/Makefile
plugins/startup-wizard/Makefile
plugins/subject-thread/Makefile
plugins/templates/Makefile
plugins/tnef-attachments/Makefile
plugins/vcard-inline/Makefile
plugins/webdav-account-setup/Makefile
smime/Makefile
smime/lib/Makefile
smime/gui/Makefile
sounds/Makefile
tools/Makefile
evolution-zip
evolution-shell.pc
evolution-plugin.pc
])

AC_OUTPUT

if test "x$with_sub_version" != "x"; then
echo "
	Evolution ($with_sub_version) has been configured as follows: "
else
echo "
	Evolution has been configured as follows: "
fi

echo "
	LDAP support:		$msg_ldap
	HAL:			$HAVE_HAL
	DBUS:			$DBUS_SUPPORT
	NetworkManager:		$NM_SUPPORT
	NNTP support:		$msg_nntp
	Pilot conduits:		$msg_pilot
	Libnotify:		$HAVE_LIBNOTIFY
	Kerberos 4/5:		$msg_krb4/$msg_krb5
	SSL support:		$msg_ssl
	SMIME support:		$msg_smime
	Plugins:		$msg_plugins
	User documentation:	$with_help
	Mono bindings:		$enable_mono
	Python bindings:	$enable_python
"

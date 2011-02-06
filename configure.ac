dnl Evolution Versions
m4_define([evo_major_version], [2])
m4_define([evo_minor_version], [32])
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
BASE_VERSION=2.32
m4_define([base_version], [2.32])

dnl Upgrade Revision: This is for triggering migration calls between
dnl varying versions. Reset to 0 whenever BASE_VERSION changes.
m4_define([upgrade_revision], [0])

dnl Autoconf / Automake Initialization
AC_PREREQ(2.58)
AC_INIT([evolution],[evo_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(README)

dnl This is for the autoconf tests only - it set's the language we use
AC_LANG(C)

dnl Automake 1.11 - Silent Build Rules
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Required Packages
m4_define([glib_minimum_version], [2.25.12])
m4_define([eds_minimum_version], [evo_version])
m4_define([gtkhtml_minimum_version], [3.31.90])
m4_define([gnome_icon_theme_minimum_version], [2.30.2.1])
m4_define([pangoft2_minimum_version], [1.0.1])		dnl For libgnomecanvas
m4_define([gconf_minimum_version], [2.0.0])		dnl XXX Just a Guess
m4_define([libsoup_minimum_version], [2.4.0])		dnl XXX Just a Guess
m4_define([libxml_minimum_version], [2.7.3])
m4_define([shared_mime_info_minimum_version], [0.22])
m4_define([libpst_minimum_version], [0.6.41])

dnl Optional Packages
m4_define([nm_minimum_version],[0.7])
m4_define([geoclue_minimum_version], [0.11.1])
m4_define([gladeui_minimum_version], [3.0.0])

dnl **********************************
dnl Gtk-3 compatibility
dnl **********************************
AC_ARG_ENABLE([gtk3],
	AS_HELP_STRING([--enable-gtk3],
	[Attempt to use gtk+-3]),
	[enable_gtk3=$enableval],[enable_gtk3=no])

if test "x${enable_gtk3}" = "xyes"; then
	LIBEDATASERVERUI="libedataserverui-3.0"
	GTK="gtk+-3.0"
	gtk_minimum_version="2.90.4"

	GAIL="gail-3.0"
	gail_minimum_version="2.90.4"

	UNIQUE="unique-3.0"
	unique_minimum_version="2.90"

	GNOME_DESKTOP="gnome-desktop-3.0"
	gnome_desktop_minimum_version="2.90"

	CANBERRA_GTK="libcanberra-gtk3"
	libcanberra_gtk_minimum_version="0.25"

	CLUTTER_GTK="clutter-gtk-1.0"
	clutter_gtk_minimum_version="0.90"

	GTKHTML="libgtkhtml-4.0"
	GTKHTML_EDITOR="gtkhtml-editor-4.0"

	GWEATHER="gweather-3.0"
	gweather_minimum_version="2.90.0"

	libnotify_minimum_version="0.5.1"

	dnl XXX These versions don't yet exist.  The latest releases
	dnl     link to gtk+-2.0 so we've bumped the versions to some
	dnl     bogus value to force some plugins to be disabled.
	champlain_minimum_version="0.8"
	gtkimageview_minimum_version="2.0"

	EVOLUTION_SHELL="evolution-shell-3.0"
else
	LIBEDATASERVERUI="libedataserverui-1.2"

	GTK="gtk+-2.0"
	gtk_minimum_version="2.20.0"

	GAIL="gail"
	gail_minimum_version="1.9.0"

	UNIQUE="unique-1.0"
	unique_minimum_version="1.1.2"

	GNOME_DESKTOP="gnome-desktop-2.0"
	gnome_desktop_minimum_version="2.26.0"

	CANBERRA_GTK="libcanberra-gtk"
	libcanberra_gtk_minimum_version="0"

	CLUTTER_GTK="clutter-gtk-0.10"
	clutter_gtk_minimum_version="0.10"

	GTKHTML="libgtkhtml-3.14"
	GTKHTML_EDITOR="gtkhtml-editor-3.14"

	GWEATHER="gweather"
	gweather_minimum_version="2.25.3"

	champlain_minimum_version="0.4"
	gtkimageview_minimum_version="1.6"
	libnotify_minimum_version="0.3.0"

	EVOLUTION_SHELL="evolution-shell"
fi

AC_SUBST(GTKHTML_EDITOR)
AC_SUBST(GTK)
AC_SUBST(GTKHTML)
AC_SUBST(UNIQUE)
AC_SUBST(GNOME_DESKTOP)
AC_SUBST(LIBEDATASERVERUI)
AC_SUBST(EVOLUTION_SHELL)
AM_CONDITIONAL([ENABLE_GTK3], [test "$enable_gtk3" = yes])

dnl ******************************
dnl Compiler Warning Flags
dnl ******************************
dnl GDK_DISABLE_DEPRECATED is excluded until
dnl we can port the GDK drawing code to cairo.
AS_COMPILER_FLAGS(WARNING_FLAGS,
	"-DG_DISABLE_DEPRECATED
	-DPANGO_DISABLE_DEPRECATED
	-DGDK_PIXBUF_DISABLE_DEPRECATED
	-DGTK_DISABLE_DEPRECATED
	-DEDS_DISABLE_DEPRECATED
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
dnl These may produce warnings we have no control over,
dnl or false positives we don't always want to see.
dnl
dnl	-Wmissing-format-attribute
dnl	-Wshadow
dnl	-Wformat-nonliteral
dnl	-Wstrict-aliasing=2
dnl	-DE_BOOK_DISABLE_DEPRECATED
dnl	-DE_CAL_DISABLE_DEPRECATED

AM_CPPFLAGS="$WARNING_FLAGS -fno-strict-aliasing"
AC_SUBST(AM_CPPFLAGS)

LDFLAGS="$LDFLAGS -Wl,--no-undefined"

AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(VERSION_COMMENT, "", [Define if you want a comment appended to the version number])

dnl *************************************
dnl Put the ACLOCAL flags in the Makefile
dnl *************************************
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

dnl ******************************
dnl Initialize maintainer mode
dnl ******************************
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
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PATH_PROG([PERL], [perl])
AC_SUBST(PERL)

AC_ARG_PROGRAM

dnl GCC 4.4 got more aggressive in its aliasing optimizations, changing
dnl behavior that -- according to the C99 standard -- is supposed to be
dnl undefined.  We may still have aliasing abuses lying around that rely
dnl on GCC's previous "undefined" behavior, so disable strict-aliasing
dnl optimzation until we can find and fix all the abuses.
dnl (AC_PROG_CC must run first to set the GCC variable.)
dnl XXX This really belongs in AM_CFLAGS.
if test "x${GCC}" = "xyes"; then
	CFLAGS="$CFLAGS -fno-strict-aliasing"
fi

dnl ******************************
dnl I18N stuff
dnl ******************************
IT_PROG_INTLTOOL([0.35.5])

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=evolution-$BASE_VERSION
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

localedir='$(prefix)/$(DATADIRNAME)/locale'
AC_SUBST(localedir)

dnl ******************************
dnl Initialize libtool
dnl ******************************
LT_PREREQ(2.2)
LT_INIT(disable-static win32-dll)

PKG_PROG_PKG_CONFIG

dnl ********************************************
dnl User / Developer Documentation
dnl ********************************************
GNOME_DOC_INIT
GTK_DOC_CHECK(1.9)

dnl ******************************
dnl Win32
dnl ******************************
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
	CHAMPLAIN_REQUIREMENT=''
	GEOCLUE_REQUIREMENT=''
	;;
*)
	os_win32=no
	NO_UNDEFINED='-no-undefined'
	SOEXT='.so'
	SA_JUNK_PLUGIN=sa-junk-plugin
	BF_JUNK_PLUGIN=bogo-junk-plugin
	DL_LIB='-ldl'
	SOFTOKN3_LIB='-lsoftokn3'
	;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, [test $os_win32 = yes])
AC_SUBST(NO_UNDEFINED)
AC_SUBST(SOEXT)

MATH_LIB="-lm"
AC_SUBST(MATH_LIB)

if test "$os_win32" = "yes"; then
	AC_CHECK_TOOL(WINDRES, windres, :)
else
	WINDRES=":"
fi

AM_CONDITIONAL(HAVE_WINDRES, test "x$WINDRES" != "x:")

AC_SUBST(WINDRES)

dnl **********************************
dnl Check for base dependencies early.
dnl **********************************
PKG_CHECK_MODULES([GNOME_PLATFORM],
	[gio-2.0 >= glib_minimum_version
	 $GTK >= $gtk_minimum_version
	 $GAIL >= $gail_minimum_version
	 gconf-2.0 >= gconf_minimum_version
	 libxml-2.0 >= libxml_minimum_version
	 pangoft2 >= pangoft2_minimum_version
	 shared-mime-info >= shared_mime_info_minimum_version
	 $GNOME_DESKTOP >= $gnome_desktop_minimum_version
	 $UNIQUE >= $unique_minimum_version])
AC_SUBST(GNOME_PLATFORM_CFLAGS)
AC_SUBST(GNOME_PLATFORM_LIBS)

PKG_CHECK_MODULES([EVOLUTION_DATA_SERVER],
	[libebook-1.2 >= eds_minimum_version
	 libecal-1.2 >= eds_minimum_version
	 libedataserver-1.2 >= eds_minimum_version
	 $LIBEDATASERVERUI >= eds_minimum_version
	 libegroupwise-1.2 >= eds_minimum_version
	 libebackend-1.2 >= eds_minimum_version])


dnl ******************************
dnl Canberra / Canberra-GTK Sound
dnl ******************************
AC_ARG_ENABLE([canberra],
	[AS_HELP_STRING([--enable-canberra],
	[Enable Canberra and Canberra-GTK sound @<:@default=yes@:>@])],
	[enable_canberra="$enableval"], [enable_canberra=yes])

if test $enable_canberra = yes; then
	PKG_CHECK_MODULES([CANBERRA],[$CANBERRA_GTK >= $libcanberra_gtk_minimum_version])
	AC_DEFINE(HAVE_CANBERRA, 1, [Define if using Canberra and Canberra-GTK for sound])
fi
AC_SUBST(CANBERRA_CFLAGS)
AC_SUBST(CANBERRA_LIBS)

dnl ******************
dnl User Documentation
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

save_CFLAGS="$CFLAGS"
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

dnl **************
dnl waitpid() etc
dnl **************
AC_HEADER_SYS_WAIT

dnl ***************
dnl Purify support
dnl ***************
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
dnl Gnome Icon Theme
dnl **************************************************
PKG_CHECK_MODULES([GIT], [gnome-icon-theme >= gnome_icon_theme_minimum_version])

dnl **************************************************
dnl Accessibility support
dnl **************************************************
PKG_CHECK_MODULES([A11Y], [atk])
AC_SUBST(A11Y_CFLAGS)
AC_SUBST(A11Y_LIBS)

dnl **************************************************
dnl XF86 multimedia keys support
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
dnl Clutter support.
dnl **************************************************
AC_MSG_CHECKING([whether to build with Clutter])
AC_ARG_WITH([clutter],
       [AC_HELP_STRING([--with-clutter],
       [Build with Clutter [default=no]])],
       with_clutter="$withval", with_clutter="no")
AC_MSG_RESULT($with_clutter)

if test "x$with_clutter" = "xyes"; then
   PKG_CHECK_MODULES(CLUTTER, [mx-1.0 $CLUTTER_GTK >= $clutter_gtk_minimum_version clutter-1.0 >= 1.0.0])

   AC_DEFINE(HAVE_CLUTTER, 1, [Clutter available])
   AM_CONDITIONAL(ENABLE_CLUTTER, true)
else
   AC_DEFINE(HAVE_CLUTTER, 0, [Clutter not available])
   AM_CONDITIONAL(ENABLE_CLUTTER, false)
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
dnl Win32 support
else
	LDAP_CFLAGS="-DLDAP_DEPRECATED"
	LDAP_LIBS="-lwldap32"
	AC_SUBST(LDAP_CFLAGS)
	AC_SUBST(LDAP_LIBS)
	AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])
	AM_CONDITIONAL(ENABLE_LDAP, true)
	msg_ldap="yes"
fi

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

AC_DEFINE(HANDLE_LIBICAL_MEMORY, 1, [Define it once memory returned by libical is free'ed properly])

dnl ************
dnl regex check
dnl ************
AC_CHECK_FUNCS(regexec,,[AC_CHECK_LIB(regex,regexec,
			[REGEX_LIBS=-lregex
			AC_DEFINE(HAVE_REGEXEC,1,[Define to 1 if you have the regexec function.])],
			[AC_MSG_ERROR([No regex library found])])])
AC_SUBST(REGEX_LIBS)

dnl *************************
dnl GTKHTML check
dnl XXX Drop the version from the package name?
dnl *************************
PKG_CHECK_MODULES([GTKHTML], [$GTKHTML >= gtkhtml_minimum_version])
AC_SUBST(GTKHTML_CFLAGS)
AC_SUBST(GTKHTML_LIBS)

dnl ************
dnl Kerberos 5
dnl ************
EVO_KRB5_SUPPORT

dnl ******************************
dnl Mono hooks
dnl This should just define mono CFLAGS etc here, it is used later to turn on the mono plugin or not.
dnl ******************************
MONO_CFLAGS=
MONO_LIBS=
AC_ARG_ENABLE([mono],
	AS_HELP_STRING([--enable-mono],
	[Add Mono embedded hooks.]),
	[enable_mono=$enableval],[enable_mono=no])
if test "x${enable_mono}" = "xyes"; then
	PKG_CHECK_MODULES([MONO], ["mono"])
	AC_DEFINE(ENABLE_MONO,1,[Define if Mono embedding should be enabled])
fi
AC_SUBST(MONO_CFLAGS)
AC_SUBST(MONO_LIBS)

AM_CONDITIONAL(ENABLE_MONO, [test "x$enable_mono" = "xyes"])

dnl ******************************
dnl Python hooks
dnl This should just define python CFLAGS etc here, it is used later to turn on the python plugin or not.
dnl (Thanks to Pidgin)
dnl ******************************

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
		else
			AC_MSG_ERROR([Can't find Python.h])
			PY_LIBS=""
			PY_INCLUDES=""
			python_package=""
		fi
	fi
	AC_SUBST(PY_LIBS)
	AC_SUBST(PY_INCLUDES)
fi

AM_CONDITIONAL(ENABLE_PYTHON, [test "x$enable_python" = "xyes"])


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
					nsprlibs="$DL_LIB $with_nspr_libs/libplc4.a $with_nspr_libs/libplds4.a $with_nspr_libs/libnspr4.a"
				fi
			else
				nsprlibs="$DL_LIB -lplc4 -lplds4 -lnspr4"
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

dnl *****************************************************************
dnl Utility macro to set compiler flags for a specific lib.
dnl EVO_SET_COMPILE_FLAGS(VAR-PREFIX, DEPS, EXTRA-CFLAGS, EXTRA-LIBS)
dnl *****************************************************************
AC_DEFUN([EVO_SET_COMPILE_FLAGS], [
	PKG_CHECK_MODULES([$1], [$2])
	$1_CFLAGS="[$]$1_CFLAGS $3"
	$1_LIBS="[$]$1_LIBS $4"
])

dnl ******************************
dnl TNEF implementation
dnl ******************************
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

dnl ******************************
dnl Check for NetworkManager
dnl ******************************
AC_ARG_ENABLE([nm],
	[AS_HELP_STRING([--enable-nm],
	[enable NetworkManager support (default=yes)])],
	[enable_nm=$enableval],[enable_nm=yes])
AC_MSG_CHECKING([if NetworkManager support is enabled])
AC_MSG_RESULT([$enable_nm])
if test "$enable_nm" = yes; then
	PKG_CHECK_MODULES([NM], [NetworkManager >= nm_minimum_version],,
	[AC_MSG_ERROR([NetworkManager not found (or version < nm_minimum_version).
	If you want to disable NetworkManager, please append --disable-nm to configure.])])
	AC_SUBST(HAVE_NM)
	AC_SUBST(NM_CFLAGS)
fi
AM_CONDITIONAL([ENABLE_NETWORK_MANAGER], [test "$enable_nm" = yes])

dnl ******************************
dnl Check for Windows SENS API
dnl ******************************
AC_ARG_ENABLE([sens],
	[AS_HELP_STRING([--enable-sens],
	[enable Windows SENS support (default=yes)])],
	[enable_sens=$enableval],[enable_sens=yes])
if test "x$os_win32" = xyes; then
	if test "x$enable_sens" = xyes; then
		AC_CHECK_HEADER([rpc.h],[ enable_sens="yes" ],[ enable_sens="no" ])
	fi
else
	enable_sens="no"
fi
AC_MSG_CHECKING([if Windows SENS support is enabled])
AC_MSG_RESULT([$enable_sens])
if test "$enable_sens" = yes; then
	SENS_LIBS="-lole32 -loleaut32 -luuid -lrpcrt4"
	AC_CHECK_HEADER([eventsys.h],
		[AC_DEFINE([HAVE_EVENTSYS_H], 1, [Have <eventsys.h>])],,
		[[	#if HAVE_EVENTSYS_H
			#include <eventsys.h>
			#endif
		]])
	AC_CHECK_HEADER([sensevts.h],
		[AC_DEFINE([HAVE_SENSEVTS_H], 1, [Have <sensevts.h>])],,
		[[	#if HAVE_SENSEVTS_H
			#include <sensevts.h>
			#endif
		]])
	AC_SUBST(HAVE_SENS)
	AC_SUBST(SENS_LIBS)
fi
AC_MSG_CHECKING([if Windows SENS support is enabled])
AC_MSG_RESULT([$enable_sens])
if test "$enable_sens" = yes; then
	SENS_LIBS="-lole32 -loleaut32 -luuid -lrpcrt4"
	AC_CHECK_HEADER([eventsys.h],
		[AC_DEFINE([HAVE_EVENTSYS_H], 1, [Have <eventsys.h>])],,
		[[	#if HAVE_EVENTSYS_H
			#include <eventsys.h>
			#endif
		]])
	AC_CHECK_HEADER([sensevts.h],
		[AC_DEFINE([HAVE_SENSEVTS_H], 1, [Have <sensevts.h>])],,
		[[	#if HAVE_SENSEVTS_H
			#include <sensevts.h>
			#endif
		]])
	AC_SUBST(HAVE_SENS)
	AC_SUBST(SENS_LIBS)
fi
AM_CONDITIONAL([ENABLE_WINDOWS_SENS], [test "$enable_sens" = yes])

dnl ******************************
dnl Enable ConnMan support ?
dnl ******************************
AC_ARG_ENABLE([connman],
	[AS_HELP_STRING([--enable-connman],
	[enable ConnMan support (default=no)])],
	[enable_connman=$enableval],[enable_connman=no])
if test "x$enable_nm$enable_connman" = "xyesyes"; then
	AC_MSG_ERROR([It is not possible to enable both ConnMan and NetworkManager])
fi
AM_CONDITIONAL([ENABLE_CONNMAN], [test "$enable_connman" = yes])

dnl ******************************
dnl Camel Flags
dnl ******************************
EVO_SET_COMPILE_FLAGS(CAMEL, camel-provider-1.2, $MANUAL_NSS_CFLAGS, $MOXILLA_NSS_CFLAGS)
AC_SUBST(CAMEL_CFLAGS)
AC_SUBST(CAMEL_LIBS)

EVO_SET_COMPILE_FLAGS(CAMEL_GROUPWISE, camel-provider-1.2 libedataserver-1.2 libegroupwise-1.2)
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

dnl ******************************
dnl CERT_UI Flags
dnl ******************************
dnl
dnl Here we want the Mozilla flags to go *before* the other ones,
dnl especially the mozilla-nss -I flags to go before the gnutls ones
dnl (which are dragged in through libedataserverui), as both
dnl gnutls and mozilla-nss have a header called "pkcs12.h" which is
dnl included in smime/lib/e-pkcs12.c. It wants the Mozilla NSS one.
dnl
EVO_SET_COMPILE_FLAGS(CERT_UI, $LIBEDATASERVERUI)
CERT_UI_CFLAGS="$MANUAL_NSS_CFLAGS $MOZILLA_NSS_CFLAGS $CERT_UI_CFLAGS"
CERT_UI_LIBS="$MOZILLA_L_DIR $MANUAL_NSS_LIBS $MOZILLA_NSS_LIBS $CERT_UI_LIBS"
AC_SUBST(CERT_UI_CFLAGS)
AC_SUBST(CERT_UI_LIBS)

dnl ************
dnl E_UTIL Flags
dnl ************
EVO_SET_COMPILE_FLAGS(E_UTIL, $LIBEDATASERVERUI camel-1.2 $mozilla_nspr, $MANUAL_NSPR_CFLAGS $MANUAL_NSPR_LIBS)
AC_SUBST(E_UTIL_CFLAGS)
AC_SUBST(E_UTIL_LIBS)

E_UTIL_LIBS="$E_UTIL_LIBS $MATH_LIB"

dnl **************
dnl TZDIALOG Flags
dnl **************
EVO_SET_COMPILE_FLAGS(TZDIALOG, libecal-1.2 camel-1.2)
AC_SUBST(TZDIALOG_CFLAGS)
AC_SUBST(TZDIALOG_LIBS)

dnl ***************
dnl E_WIDGETS Flags
dnl ***************
EVO_SET_COMPILE_FLAGS(E_WIDGETS, $LIBEDATASERVERUI $GTKHTML_EDITOR)
AC_SUBST(E_WIDGETS_CFLAGS)
AC_SUBST(E_WIDGETS_LIBS)

dnl ***************
dnl IMPORTERS Flags
dnl ***************
EVO_SET_COMPILE_FLAGS(IMPORTERS, $LIBEDATASERVERUI camel-provider-1.2 libebook-1.2)
AC_SUBST(IMPORTERS_CFLAGS)
AC_SUBST(IMPORTERS_LIBS)

dnl ***************
dnl LIBFILTER Flags
dnl ***************
EVO_SET_COMPILE_FLAGS(LIBFILTER, libedataserver-1.2 camel-1.2)
AC_SUBST(LIBFILTER_CFLAGS)
AC_SUBST(LIBFILTER_LIBS)

dnl ***********
dnl Shell Flags
dnl ***********
EVO_SET_COMPILE_FLAGS(SHELL, $GTKHTML $LIBEDATASERVERUI)
AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

dnl *****************
dnl Addressbook Flags
dnl *****************
EVOLUTION_ADDRESSBOOK_DEPS="$GTKHTML libebook-1.2 $LIBEDATASERVERUI camel-1.2 $GTKHTML_EDITOR"
EVO_SET_COMPILE_FLAGS(EVOLUTION_ADDRESSBOOK, $EVOLUTION_ADDRESSBOOK_DEPS)
AC_SUBST(EVOLUTION_ADDRESSBOOK_CFLAGS)
AC_SUBST(EVOLUTION_ADDRESSBOOK_LIBS)

dnl ***************
dnl Libnotify Flags
dnl ***************
LIBNOTIFY_CFLAGS=
LIBNOTIFY_LIBS=
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= $libnotify_minimum_version], [HAVE_LIBNOTIFY="yes"], [HAVE_LIBNOTIFY="no"])
if test "x$HAVE_LIBNOTIFY" = "xyes"; then
	AC_DEFINE(HAVE_LIBNOTIFY, 1, [libnotify available])
	libnotify="libnotify"
else
	libnotify=""
fi
AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)

dnl **************
dnl Calendar Flags
dnl **************
EVO_SET_COMPILE_FLAGS(LIBSOUP, libsoup-2.4 >= libsoup_minimum_version)
AC_SUBST(LIBSOUP_CFLAGS)
AC_SUBST(LIBSOUP_LIBS)

EVO_SET_COMPILE_FLAGS(GDATA, libgdata >= 0.4.0)
AC_SUBST(GDATA_CFLAGS)
AC_SUBST(GDATA_LIBS)

EVO_SET_COMPILE_FLAGS(EVOLUTION_CALENDAR, $GTKHTML libebook-1.2 libecal-1.2 $LIBEDATASERVERUI libebackend-1.2 $libnotify $GTKHTML_EDITOR)
AC_SUBST(EVOLUTION_CALENDAR_CFLAGS)
AC_SUBST(EVOLUTION_CALENDAR_LIBS)

dnl ****************************
dnl Check for ical_set_unknown_token_handling_setting function
dnl ****************************
AC_MSG_CHECKING([ical_set_unknown_token_handling_setting function])
save_cflags=$CFLAGS; CFLAGS=$EVOLUTION_CALENDAR_CFLAGS
save_libs=$LIBS; LIBS="$EVOLUTION_CALENDAR_LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[[#include <libical/ical.h>]],
	[[ical_set_unknown_token_handling_setting (ICAL_DISCARD_TOKEN)]])],
	[AC_DEFINE(HAVE_ICAL_UNKNOWN_TOKEN_HANDLING, 1, [libical provides ical_set_unknown_token_handling_setting function]) ac_cv_have_iuth=yes],[ac_cv_have_iuth=no])
CFLAGS=$save_cflags
LIBS=$save_libs
AC_MSG_RESULT([$ac_cv_have_iuth])

dnl **********
dnl Mail Flags
dnl **********
EVO_SET_COMPILE_FLAGS(EVOLUTION_MAIL, camel-provider-1.2 $GTKHTML $GTKHTML_EDITOR $mozilla_nss libebook-1.2 $LIBEDATASERVERUI)
AC_SUBST(EVOLUTION_MAIL_CFLAGS)
AC_SUBST(EVOLUTION_MAIL_LIBS)

dnl ***************************
dnl Evolution-Data-Server Flags
dnl ***************************
DATASERVER_EXEC_VERSION=`${PKG_CONFIG} --variable=execversion evolution-data-server-1.2`
AC_SUBST(DATASERVER_EXEC_VERSION)

dnl *****************
dnl EggSMClient Flags
dnl *****************
if test "x$os_win32" = "xyes"; then
	EVO_SET_COMPILE_FLAGS(EGG_SMCLIENT, $GTK)
else
	EVO_SET_COMPILE_FLAGS(EGG_SMCLIENT, $GTK,, -lSM -lICE)
fi
AC_SUBST(EGG_SMCLIENT_CFLAGS)
AC_SUBST(EGG_SMCLIENT_LIBS)

dnl *******************
dnl Special directories
dnl *******************

dnl If you add something here, consider whether or not you also need to add it to one or more .pc.in
dnl files (for Connector, etc)

privlibdir='${libdir}'/evolution/$BASE_VERSION
AC_SUBST(privlibdir)

privlibexecdir='${libexecdir}'/evolution/$BASE_VERSION
AC_SUBST(privlibexecdir)

privdatadir='${datadir}'/evolution/$BASE_VERSION
AC_SUBST(privdatadir)

privincludedir='${includedir}'/evolution-$BASE_VERSION
AC_SUBST(privincludedir)

moduledir="$privlibdir/modules"
AC_SUBST(moduledir)

uidir="$privdatadir/ui"
AC_SUBST(uidir)

evolutionhelpdir="$privdatadir/help"
AC_SUBST(evolutionhelpdir)

icondir="$privdatadir/icons"
AC_SUBST(icondir)

imagesdir="$privdatadir/images"
AC_SUBST(imagesdir)

if test "$os_win32" = yes; then
	dnl On Win32 there is no "rpath" mechanism. We install the private
	dnl shared libraries in $libdir, meaning the DLLs will actually be in
	dnl $bindir. This means just having $bindir in PATH will be enough. This
	dnl also means gnome_win32_get_prefixes() will be able to deduce the
	dnl installation folder correctly.
	privsolibdir=$libdir
else
	privsolibdir=$privlibdir
fi
AC_SUBST(privsolibdir)

soundsdir="$privdatadir/sounds"
AC_SUBST(soundsdir)

etspecdir="$privdatadir/etspec"
AC_SUBST(etspecdir)

viewsdir="$privdatadir/views"
AC_SUBST(viewsdir)

dnl For evolution-alarm-notify.desktop
AS_AC_EXPAND(PRIVLIBEXECDIR, "$privlibexecdir")

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
plugins_base_always="calendar-file calendar-http itip-formatter plugin-manager default-source addressbook-file mark-all-read groupwise-features publish-calendar caldav imap-features google-account-setup webdav-account-setup"

plugins_base="$plugins_base_always $SA_JUNK_PLUGIN $BF_JUNK_PLUGIN"
dist_plugins_base="$plugins_base_always calendar-weather sa-junk-plugin bogo-junk-plugin"

plugins_standard_always="bbdb subject-thread save-calendar mail-to-task mailing-list-actions prefer-plain mail-notification attachment-reminder backup-restore email-custom-header templates vcard-inline dbx-import"

plugins_standard="$plugins_standard_always"
dist_plugins_standard="$plugins_standard audio-inline image-inline pst-import"

plugins_experimental_always="face external-editor"

plugins_experimental="$plugins_experimental_always $TNEF_ATTACHMENTS"
dist_plugins_experimental="$plugins_experimental_always profiler tnef-attachments contacts-map"

dnl ******************************
dnl Profiling support
dnl ******************************
AC_ARG_ENABLE([profiling],
	AS_HELP_STRING([--enable-profiling],
	[Enable profiling plugin.]),
	[enable_profiling=$enableval],[enable_profiling=no])

case x"$enable_profiling" in
x | xyes)
	plugins_experimental="$plugins_experimental profiler"
	AC_DEFINE(ENABLE_PROFILING,1,[Profiling Hooks Enabled])
	;;
esac

dnl ******************************************************************
dnl The following plugins have additional library dependencies.
dnl They must be explicitly disabled if the libraries are not present.
dnl ******************************************************************

dnl *******************************************
dnl audio-inline plugin requires gstreamer-0.10
dnl *******************************************
AC_ARG_ENABLE([audio-inline],
	[AS_HELP_STRING([--enable-audio-inline],
	[Enable audio-inline plugin @<:@default=yes@:>@])],
	[enable_audio_inline="$enableval"], [enable_audio_inline=yes])

if test "x$enable_audio_inline" = "xyes"; then
	PKG_CHECK_MODULES(GSTREAMER, gstreamer-0.10, have_gst=yes, have_gst=no)
	AC_SUBST(GSTREAMER_CFLAGS)
	AC_SUBST(GSTREAMER_LIBS)

	if test "x$have_gst" = "xyes"; then
		plugins_standard="$plugins_standard audio-inline"
	else
		AC_MSG_ERROR([gstreamer-0.10 is required for the audio-inline plugin.  Use --disable-audio-inline to exclude the plugin.])
	fi
fi

dnl *****************************************
dnl calendar-weather plugin requires gweather
dnl *****************************************
AC_ARG_ENABLE([weather],
	[AS_HELP_STRING([--enable-weather],
	[Enable calendar-weather plugin @<:@default=yes@:>@])],
	[enable_weather="$enableval"],[enable_weather=yes])

if test "x$enable_weather" = "xyes"; then
	PKG_CHECK_MODULES([LIBGWEATHER], $GWEATHER >= $gweather_minimum_version,
	have_weather="yes", have_weather="no")
	AC_SUBST(GWEATHER_CFLAGS)
	AC_SUBST(GWEATHER_LIBS)

	if test "x$have_weather" = "xyes"; then
		plugins_base="$plugins_base calendar-weather"
	else
		AC_MSG_ERROR([$GWEATHER >= $gweather_minimum_version is required for the calendar-weather plugin.  Use --disable-weather to exclude the plugin.])
	fi
fi

dnl ******************************************************
dnl contacts-map plugin requires champlain-gtk and geoclue
dnl N.B. The plugin is experimental so only require these
dnl      packages if we are building experimental plugins.
dnl ******************************************************
if test "x$enable_plugins" = "xexperimental"; then
	AC_ARG_ENABLE([contacts-map],
		[AS_HELP_STRING([--enable-contacts-map],
		[Enable contacts-map plugin @<:@default=yes@:>@])],
		[enable_contacts_map="$enableval"], [enable_contacts_map=yes])

	if test "x$enable_contacts_map" = "xyes"; then
		PKG_CHECK_MODULES([CHAMPLAIN], [champlain-gtk-0.5 >= $champlain_minimum_version], [have_champlain=yes], [have_champlain=no])
		if test "x$have_champlain" = "xno"; then
			PKG_CHECK_MODULES([CHAMPLAIN], [champlain-gtk-0.4 >= $champlain_minimum_version], [have_champlain=yes], [have_champlain=no])
		fi
		AC_SUBST(CHAMPLAIN_CFLAGS)
		AC_SUBST(CHAMPLAIN_LIBS)

		if test "x$have_champlain" = "xno"; then
			AC_MSG_ERROR([champlain-gtk >= $champlain_minimum_version is required for the contacts-map plugin.  Use --disable-contacts-map to exclude the plugin.])
		fi

		PKG_CHECK_MODULES([GEOCLUE], [geoclue >= geoclue_minimum_version], [have_geoclue=yes], [have_geoclue=no])
		AC_SUBST(GEOCLUE_CFLAGS)
		AC_SUBST(GEOCLUE_LIBS)

		if test "x$have_geoclue" = "xno"; then
			AC_MSG_ERROR([geoclue is required for the contacts-map plugin.  Use --disable-contacts-map to exclude the plugin.])
		fi

		PKG_CHECK_MODULES( [CLUTTER_GTK], [$CLUTTER_GTK >= $clutter_gtk_minimum_version], [have_clutter_gtk="yes"], [have_clutter_gtk="no"] )

		if test "x$have_clutter_gtk" = "xno"; then
			AC_MSG_ERROR([$CLUTTER_GTK is required for the contacts-map plugin.  Use --disable-contacts-map to exclude the plugin.])
		fi

		plugins_standard="$plugins_standard contacts-map"
	fi
fi

dnl *****************************************
dnl image-inline plugin requires gtkimageview
dnl *****************************************
AC_ARG_ENABLE([image-inline],
	[AS_HELP_STRING([--enable-image-inline],
	[Enable image-inline plugin @<:@default=yes@:>@])],
	[enable_image_inline="$enableval"], [enable_image_inline=yes])

if test "x$enable_image_inline" = "xyes"; then
	PKG_CHECK_MODULES(GTKIMAGEVIEW, gtkimageview >= $gtkimageview_minimum_version, have_imageview=yes, have_imageview=no)
	AC_SUBST(GTKIMAGEVIEW_CFLAGS)
	AC_SUBST(GTKIMAGEVIEW_LIBS)

	if test "x$have_imageview" = "xyes"; then
		plugins_standard="$plugins_standard image-inline"
	else
		AC_MSG_ERROR([gtkimageview >= $gtkimageview_minimum_version is required for the image-inline plugin.  Use --disable-image-inline to exclude the plugin.])
	fi
fi

dnl *********************************
dnl pst-import plugin requires libpst
dnl *********************************
AC_ARG_ENABLE([pst-import],
	[AS_HELP_STRING([--enable-pst-import],
	[Enable pst-import plugin @<:@default=yes@:>@])],
	[enable_pst="$enableval"], [enable_pst=yes])

if test "x$enable_pst" = "xyes"; then
	PKG_CHECK_MODULES(LIBPST, libpst, have_pst=yes, have_pst=no)
	AC_SUBST(LIBPST_CFLAGS)
	AC_SUBST(LIBPST_LIBS)

	if test "x$have_pst" = "xyes"; then
		plugins_standard="$plugins_standard pst-import"
	else
		AC_MSG_ERROR([libpst is required for the pst-import plugin.  Use --disable-pst-import to exclude the plugin.])
	fi
fi

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

AC_SUBST(plugins_enabled)
AC_SUBST(dist_plugins_base)
AC_SUBST(dist_plugins_standard)
AC_SUBST(dist_plugins_experimental)

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

dnl ********************************************
dnl Glade 3 catalog files (for maintainers only)
dnl ********************************************
AC_ARG_WITH([glade-catalog],
	[AS_HELP_STRING([--with-glade-catalog],
	[Install the catalog files for Glade 3 ]
	[(for maintainers only) [default=no]])],
	[with_catalog="$withval"], [with_catalog="no"])
if test "$with_catalog" = "yes"; then
	PKG_CHECK_MODULES(GLADEUI, [gladeui-1.0 >= gladeui_minimum_version])
fi
AM_CONDITIONAL(GLADE_CATALOG, test "x$with_catalog" != "xno")

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
a11y/Makefile
addressbook/Makefile
addressbook/gui/Makefile
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
data/evolution.desktop.in
data/evolution-alarm-notify.desktop.in
data/evolution-settings.desktop.in
data/icons/Makefile
doc/Makefile
doc/reference/Makefile
doc/reference/shell/Makefile
e-util/Makefile
em-format/Makefile
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
libart_lgpl/Makefile
libgnomecanvas/Makefile
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
calendar/gui/Makefile
calendar/gui/alarm-notify/Makefile
calendar/gui/dialogs/Makefile
composer/Makefile
m4/Makefile
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
mail/default/id/Makefile
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
maint/Makefile
modules/Makefile
modules/addressbook/Makefile
modules/calendar/Makefile
modules/mail/Makefile
modules/mailto-handler/Makefile
modules/network-manager/Makefile
modules/composer-autosave/Makefile
modules/connman/Makefile
modules/plugin-lib/Makefile
modules/plugin-mono/Makefile
modules/plugin-python/Makefile
modules/startup-wizard/Makefile
modules/windows-sens/Makefile
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
plugins/dbx-import/Makefile
plugins/default-source/Makefile
plugins/email-custom-header/Makefile
plugins/external-editor/Makefile
plugins/face/Makefile
plugins/google-account-setup/Makefile
plugins/groupwise-features/Makefile
plugins/image-inline/Makefile
plugins/imap-features/Makefile
plugins/itip-formatter/Makefile
plugins/mail-notification/Makefile
plugins/mail-to-task/Makefile
plugins/mailing-list-actions/Makefile
plugins/mark-all-read/Makefile
plugins/plugin-manager/Makefile
plugins/prefer-plain/Makefile
plugins/profiler/Makefile
plugins/pst-import/Makefile
plugins/publish-calendar/Makefile
plugins/sa-junk-plugin/Makefile
plugins/save-calendar/Makefile
plugins/subject-thread/Makefile
plugins/templates/Makefile
plugins/tnef-attachments/Makefile
plugins/vcard-inline/Makefile
plugins/webdav-account-setup/Makefile
plugins/contacts-map/Makefile
smclient/Makefile
smime/Makefile
smime/lib/Makefile
smime/gui/Makefile
sounds/Makefile
capplet/Makefile
capplet/settings/Makefile
capplet/settings/mail-autoconfig/Makefile
evolution-zip
evolution-calendar.pc
evolution-mail.pc
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
	GTK+ package:		$GTK
	LDAP support:		$msg_ldap
	Clutter support:	$with_clutter
	NetworkManager:		$enable_nm
	Windows SENS:           $enable_sens
	ConnMan:		$enable_connman
	Libnotify:		$HAVE_LIBNOTIFY
	Kerberos 5:		$msg_krb5
	SSL support:		$msg_ssl
	SMIME support:		$msg_smime
	Plugins:		$msg_plugins
	User documentation:	$with_help
	Mono bindings:		$enable_mono
	Python bindings:	$enable_python
"

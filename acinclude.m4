# evolution/acinclude.m4
# shared configure.in hacks between Evolution and Connector

# EVO_CHECK_LIB(dispname, pkgname, minvers[, maxvers])
# Checks if the package with human-readable name @dispname, known
# to gnome-config as @pkgname exists and has an appropriate version.
# The version must be >= @minvers. If @maxvers is equal to @minvers,
# it must be exactly that version. Otherwise, if @maxvers is set,
# the version must be LESS THAN @maxvers (not less than or equal).
AC_DEFUN(EVO_CHECK_LIB, [
	dispname="$1"
	pkgname="$2"
	minvers="$3"
	maxvers="$4"

	AC_MSG_CHECKING(for $dispname)

	if gnome-config --libs $pkgname > /dev/null 2>&1; then
		pkgvers=`gnome-config --modversion $pkgname | sed -e 's/^[[^0-9]]*//'`
	else
		pkgvers=not
	fi
	AC_MSG_RESULT($pkgvers found)

	pkgvers=`echo $pkgvers | awk -F. '{ print $[]1 * 1000000 + $[]2 * 10000 + $[]3 * 100 + $[]4;}'`
	cmpminvers=`echo $minvers | awk -F. '{ print $[]1 * 1000000 + $[]2 * 10000 + $[]3 * 100 + $[]4;}'`
	cmpmaxvers=`echo $maxvers | awk -F. '{ print $[]1 * 1000000 + $[]2 * 10000 + $[]3 * 100 + $[]4;}'`
	ok=yes
	if test "$pkgvers" -lt $cmpminvers; then
		ok=no
	elif test -n "$maxvers"; then
		if test "$pkgvers" -gt $cmpmaxvers; then
			ok=no
		elif test "$maxvers" != "$minvers" -a "$cmpmaxvers" -eq "$pkgvers"; then
			ok=no
		fi
	fi
	if test $ok = no; then
		case $maxvers in
		"")
			dispvers="$minvers or higher"
			;;
		$minvers)
			dispvers="$minvers (exactly)"
			;;
		*)
			dispvers="$minvers or higher, but less than $maxvers,"
			;;
		esac

		AC_MSG_ERROR([
""
"You need $dispname $dispvers to build $PACKAGE"
"If you think you already have this installed, consult the README."])
	fi
])


# EVO_PURIFY_SUPPORT
# Add --enable-purify. If the user turns it on, subst PURIFY and set
# the automake conditional ENABLE_PURIFY
AC_DEFUN(EVO_PURIFY_SUPPORT, [
	AC_ARG_ENABLE(purify, 
	[  --enable-purify=[no/yes]      Enable support for building executables with Purify.],,enable_purify=no)
	AC_PATH_PROG(PURIFY, purify, impure)
	AC_ARG_WITH(purify-options, [  --with-purify-options=OPTIONS      Options passed to the purify command line (defaults to PURIFYOPTIONS variable).])
	if test "x$with_purify_options" = "xno"; then
		with_purify_options="-always-use-cache-dir=yes -cache-dir=/gnome/lib/purify"
	fi
	if test "x$PURIFYOPTIONS" = "x"; then
		PURIFYOPTIONS=$with_purify_options
	fi
	AC_SUBST(PURIFY)
	AM_CONDITIONAL(ENABLE_PURIFY, test "x$enable_purify" = "xyes" -a "x$PURIFY" != "ximpure")
	PURIFY="$PURIFY $PURIFYOPTIONS"
])


# EVO_LDAP_CHECK(default)
# Add --with-openldap and --with-static-ldap options. --with-openldap
# defaults to the given value if not specified. If LDAP support is
# configured, HAVE_LDAP will be defined and the automake conditional
# ENABLE_LDAP will be set. LDAP_CFLAGS and LDAP_LIBS will be set
# appropriately.
AC_DEFUN(EVO_LDAP_CHECK, [
	default="$1"

	AC_ARG_WITH(openldap,     [  --with-openldap=[no/yes/PREFIX]      Enable LDAP support in evolution])
	AC_ARG_WITH(static-ldap,  [  --with-static-ldap=[no/yes]          Link LDAP support statically into evolution ])
	AC_CACHE_CHECK([for OpenLDAP], ac_cv_with_openldap, ac_cv_with_openldap="${with_openldap:=$default}")
	case $ac_cv_with_openldap in
	no|"")
		with_openldap=no
		;;
	yes)
		with_openldap=/usr
		;;
	*)
		with_openldap=$ac_cv_with_openldap
		LDAP_CFLAGS="-I$ac_cv_with_openldap/include"
		LDAP_LDFLAGS="-L$ac_cv_with_openldap/lib"
		;;
	esac

	if test "$with_openldap" != no; then
		AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])

		case $with_static_ldap in
		no|"")
			with_static_ldap=no
			;;
		*)
			with_static_ldap=yes
			;;
		esac

		AC_CACHE_CHECK(if OpenLDAP is version 2.x, ac_cv_openldap_version2, [
			CPPFLAGS_save="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $LDAP_CFLAGS"
			AC_EGREP_CPP(yes, [
				#include "ldap.h"
				#if LDAP_VENDOR_VERSION > 20000
				yes
				#endif
			], ac_cv_openldap_version2=yes, ac_cv_openldap_version2=no)
			CPPFLAGS="$CPPFLAGS_save"
		])
		if test "$ac_cv_openldap_version2" = no; then
			AC_MSG_ERROR(evolution requires OpenLDAP version >= 2)
		fi

		AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="-lresolv")
		AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
		AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
		AC_CHECK_LIB(lber, ber_get_tag, [
			if test "$with_static_ldap" = "yes"; then
				LDAP_LIBS="$with_openldap/lib/liblber.a $LDAP_LIBS"

				# libldap might depend on OpenSSL... We need to pull
				# in the dependency libs explicitly here since we're
				# not using libtool for the configure test.
				if test -f $with_openldap/lib/libldap.la; then
					LDAP_LIBS="`. $with_openldap/lib/libldap.la; echo $dependency_libs` $LDAP_LIBS"
				fi
			else
				LDAP_LIBS="-llber $LDAP_LIBS"
			fi
			AC_CHECK_LIB(ldap, ldap_open, [
					if test $with_static_ldap = "yes"; then
						LDAP_LIBS="$with_openldap/lib/libldap.a $LDAP_LIBS"
					else
						LDAP_LIBS="-lldap $LDAP_LIBS"
					fi],
				LDAP_LIBS="", $LDAP_LDFLAGS $LDAP_LIBS)
			LDAP_LIBS="$LDAP_LDFLAGS $LDAP_LIBS"
		], LDAP_LIBS="", $LDAP_LDFLAGS $LDAP_LIBS)

		if test -z "$LDAP_LIBS"; then
			AC_MSG_ERROR(could not find OpenLDAP libraries)
		fi

		AC_SUBST(LDAP_CFLAGS)
		AC_SUBST(LDAP_LIBS)
	fi
	AM_CONDITIONAL(ENABLE_LDAP, test $with_openldap != no)
])

# EVO_PTHREAD_CHECK
AC_DEFUN([EVO_PTHREAD_CHECK],[
	PTHREAD_LIB=""
	AC_CHECK_LIB(pthread, pthread_create, PTHREAD_LIB="-lpthread",
		[AC_CHECK_LIB(pthreads, pthread_create, PTHREAD_LIB="-lpthreads",
		    [AC_CHECK_LIB(c_r, pthread_create, PTHREAD_LIB="-lc_r",
			[AC_CHECK_LIB(pthread, __pthread_attr_init_system, PTHREAD_LIB="-lpthread",
				[AC_CHECK_FUNC(pthread_create)]
			)]
		    )]
		)]
	)
	AC_SUBST(PTHREAD_LIB)
	AC_PROVIDE([EVO_PTHREAD_CHECK])
])

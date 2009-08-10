dnl EVO_SUNLDAP_CHECK
dnl Add --with-sunldap and --with-static-sunldap options. --with-sunldap
dnl defaults to the given value if not specified. If LDAP support is
dnl configured, HAVE_LDAP will be defined and the automake conditional
dnl ENABLE_LDAP will be set. LDAP_CFLAGS and LDAP_LIBS will be set
dnl appropriately, and --with-sunldap and --with-openldap is mutually exclusive.
#serial 0.1
AC_DEFUN([EVO_SUNLDAP_CHECK], [
	default="$1"

	AC_ARG_WITH([sunldap],
		[AS_HELP_STRING([--with-sunldap],
		[Enable SunLDAP support in evolution])])
	AC_ARG_WITH([static-sunldap],
		[AS_HELP_STRING([--with-static-sunldap],
		[Link SunLDAP support statically into evolution])])
	AC_CACHE_CHECK([for SunLDAP],[ac_cv_with_sunldap],[ac_cv_with_sunldap="${with_sunldap:=$default}"])
	case $ac_cv_with_sunldap in
	no|"")
		with_sunldap=no
		;;
	yes)
		with_sunldap=/usr
		;;
	*)
		with_sunldap=$ac_cv_with_sunldap
		LDAP_CFLAGS="-I$ac_cv_with_sunldap/include"
		LDAP_LDFLAGS="-L$ac_cv_with_sunldap/lib"
		;;
	esac

	if test "$with_sunldap" != no; then
		AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])
		AC_DEFINE(SUNLDAP, 1, [Define if you use SunLDAP])

		case $with_static_sunldap in
		no|"")
			with_static_sunldap=no
			;;
		*)
			with_static_sunldap=yes
			;;
		esac

		AC_CACHE_CHECK([if SunLDAP is version 2.x], [ac_cv_sunldap_version2], [
			CPPFLAGS_save="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $LDAP_CFLAGS"
			AC_EGREP_CPP(yes, [
				#include "ldap.h"
				#if LDAP_VENDOR_VERSION >= 500
				yes
				#endif
			],[ac_cv_sunldap_version2=yes],[ac_cv_sunldap_version2=no])
			CPPFLAGS="$CPPFLAGS_save"
		])
		if test "$ac_cv_sunldap_version2" = no; then
		       AC_MSG_ERROR([evolution requires SunLDAP version >= 2])
		fi

		AC_CHECK_LIB(resolv, res_query, [LDAP_LIBS="-lresolv"])
		AC_CHECK_LIB(socket, bind, [LDAP_LIBS="$LDAP_LIBS -lsocket"])
		AC_CHECK_LIB(nsl, gethostbyaddr, [LDAP_LIBS="$LDAP_LIBS -lnsl"])
		AC_CHECK_LIB(ldap, ldap_open, [
			if test $with_static_sunldap = "yes"; then
				LDAP_LIBS="$with_sunldap/lib/libldap.a $LDAP_LIBS"
			else
				LDAP_LIBS="-lldap $LDAP_LIBS"
			fi
			if test `uname -s` != "SunOS" ; then
				AC_CHECK_LIB(lber, ber_get_tag, [
					if test "$with_static_sunldap" = "yes"; then
						LDAP_LIBS="$with_sunldap/lib/liblber.a $LDAP_LIBS"
						# libldap might depend on OpenSSL... We need to pull
						# in the dependency libs explicitly here since we're
						# not using libtool for the configure test.
						if test -f $with_sunldap/lib/libldap.la; then
							LDAP_LIBS="`. $with_sunldap/lib/libldap.la; echo $dependency_libs` $LDAP_LIBS"
						fi
					else
						LDAP_LIBS="-llber $LDAP_LIBS"
					fi], [LDAP_LIBS=""], [$LDAP_LDFLAGS $LDAP_LIBS])
			fi
			LDAP_LIBS="$LDAP_LDFLAGS $LDAP_LIBS"
		], [LDAP_LIBS=""], [$LDAP_LDFLAGS $LDAP_LIBS])

		if test -z "$LDAP_LIBS"; then
			AC_MSG_ERROR([could not find SunLDAP libraries])
		fi

		AC_SUBST(LDAP_CFLAGS)
		AC_SUBST(LDAP_LIBS)
	fi
	AM_CONDITIONAL(ENABLE_LDAP, test "$with_sunldap" != "no")
])

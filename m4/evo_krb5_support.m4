dnl EVO_KRB5_SUPPORT(default)
dnl Add --with-krb5, --with-krb5-libs and --with-krb5-include options.
dnl --with-krb5 defaults to the given value if not specified.
#serial 0.2
AC_DEFUN([EVO_KRB5_SUPPORT],[
	dnl ******************************
	dnl Kerberos
	dnl ******************************
	default="$1"
	AC_ARG_WITH([krb5],
		AS_HELP_STRING([--with-krb5=PATH],
		[Location of Kerberos 5 install dir]))

	AC_ARG_WITH([krb5-libs],
		AS_HELP_STRING([--with-krb5-libs=PATH],
		[Location of Kerberos 5 libraries]))

	AC_ARG_WITH([krb5-includes],
		AS_HELP_STRING([--with-krb5-includes=PATH],
		[Location of Kerberos 5 headers]))

	dnl ******************************
	dnl Kerberos 5
	dnl ******************************
	msg_krb5="no"
	AC_MSG_CHECKING([for Kerberos 5])
	with_krb5="${with_krb5:=$default}"
	case $with_krb5 in
		no|"")
			with_krb5=no
			;;
		yes)
			with_krb5=/usr
			;;
		*)
			with_krb5=$with_krb5
			;;
	esac

	if test "x${with_krb5}" != "xno"; then
		LIBS_save="$LIBS"

		case $with_krb5_libs in
			yes|no|"")
				with_krb5_libs=$with_krb5/lib
				;;
			*)
				with_krb5_libs=$with_krb5_libs
				;;
		esac

		case $with_krb5_includes in
			yes|no|"")
				with_krb5_includes=$with_krb5/include
				;;
			*)
				with_krb5_includes=$with_krb5_includes
				;;
		esac

		mitlibs="-lkrb5 -lk5crypto -lcom_err -lgssapi_krb5"
		heimlibs="-lkrb5 -lcrypto -lasn1 -lcom_err -lroken -lgssapi"
		sunlibs="-lkrb5 -lgss"
		AC_CACHE_VAL([ac_cv_lib_kerberos5],
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
		else
			AC_MSG_ERROR([You specified with krb5, but it was not found.])
		fi
	else
		msg_krb5="no"
	fi
	AC_MSG_RESULT([$msg_krb5])

	AM_CONDITIONAL(ENABLE_KRB5, [test "x$with_krb5" != "xno"])

	AC_CHECK_HEADER([et/com_err.h],
		[AC_DEFINE([HAVE_ET_COM_ERR_H], 1, [Have <et/com_err.h>])],,
		[[	#if HAVE_ET_COM_ERR_H
			#include <com_err.h>
			#endif
		]])
	AC_CHECK_HEADER([com_err.h],
		[AC_DEFINE([HAVE_COM_ERR_H], 1, [Have <com_err.h>])],,
		[[	#if HAVE_COM_ERR_H
			#include <com_err.h>
			#endif
		]])

	AC_SUBST(KRB5_CFLAGS)
	AC_SUBST(KRB5_LIBS)
])

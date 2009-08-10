dnl PILOT_LINK_CHECK
dnl Adds --with-pisock and determines the verion of the pisock
#serial 0.1
AC_SUBST(PISOCK_CFLAGS)
AC_SUBST(PISOCK_LIBS)

AC_DEFUN([PILOT_LINK_HOOK],[
	AC_ARG_WITH([pisock],
		AS_HELP_STRING([--with-pisock=PREFIX],
		[Specify prefix for pisock files]),
	[
	if test x$withval = xyes; then
		dnl Note that an empty true branch is not valid sh syntax.
		ifelse([$1], [], :, [$1])
	else
		PISOCK_CFLAGS="-I$withval/include"
		incdir="$withval/include"
		PISOCK_LIBS="-L$withval/lib -lpisock -lpisync"
		AC_MSG_CHECKING([for existance of "$withval"/lib/libpisock.so])
		if test -r $withval/lib/libpisock.so; then
		AC_MSG_RESULT([yes])
		else
		AC_MSG_ERROR([Unable to find libpisock. Try  http://www.pilot-link.org.])
		fi
	fi
	])

	if test x$PISOCK_CFLAGS = x; then
		AC_CHECK_HEADER(pi-version.h, [incdir="/usr/include"], [
		AC_CHECK_HEADER(libpisock/pi-version.h, [PISOCK_CFLAGS="-I/usr/include/libpisock"
							piversion_include="libpisock/pi-version.h"
							incdir="/usr/include/libpisock"
							],[
		AC_CHECK_HEADER($prefix/include/pi-version.h, [PISOCK_CFLAGS="-I$prefix/include/libpisock"
								piversion_include="$prefix/include/pi-version.h"
								if test x$PISOCK_LIBDIR = x; then
									incdir="$prefix/include"
									PISOCK_LIBS="-L$prefix/lib -lpisock -lpisync"
								fi
								],
		AC_MSG_ERROR([Unable to find pi-version.h]))
		])
		])
	fi

	if test "x$PISOCK_LIBS" = "x"; then
		AC_CHECK_LIB(pisock, pi_accept, [ PISOCK_LIBS="-lpisock -lpisync"],
			[ AC_MSG_ERROR([Unable to find libpisock. Try http://www.pilot-link.org.]) ])
	fi

	AC_ARG_ENABLE([pilotlinktest],
		AS_HELP_STRING([--enable-pilotlinktest],
		[Test for correct version of pilot-link]),
		[testplversion="$enableval"],[testplversion=yes]
	)

	if test x$piversion_include = x; then
		piversion_include="pi-version.h"
	fi

	pi_major=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_VERSION'|sed 's/#define PILOT_LINK_VERSION \([[0-9]]*\)/\1/'`
	pi_minor=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_MAJOR'|sed 's/#define PILOT_LINK_MAJOR \([[0-9]]*\)/\1/'`
	pi_micro=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_MINOR'|sed 's/#define PILOT_LINK_MINOR \([[0-9]]*\)/\1/'`
	pi_patch=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_PATCH'|sed 's/#define PILOT_LINK_PATCH \"\(.*\)\"/\1/'`

	PILOT_LINK_MAJOR="$pi_major"
	PILOT_LINK_MINOR="$pi_minor"
	PILOT_LINK_MICRO="$pi_micro"
	PILOT_LINK_PATCH="$pi_patch"
	PILOT_LINK_VERSION="$pi_major.$pi_minor.$pi_micro$pi_patch"

	if test x$testplversion = xyes; then
		AC_MSG_CHECKING([for pilot-link version >= $1])
		pl_ma=`echo $1|sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
		pl_mi=`echo $1|sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
		pl_mc=`echo $1|sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
		CFLAGS_save="$CFLAGS"
		CFLAGS="$CFLAGS $PISOCK_CFLAGS"
		AC_TRY_RUN(
			[
			#include <$piversion_include>
			int main(int argc,char *argv[]) {
				if (PILOT_LINK_VERSION == $pl_ma) {
					if (PILOT_LINK_MAJOR == $pl_mi) {
						if (PILOT_LINK_MINOR >= $pl_mc) {
							return 0;
							}
					} else if (PILOT_LINK_MAJOR > $pl_mi) {
						return 0;
					}
				} else if (PILOT_LINK_VERSION > $pl_ma) {
					return 0;
				}
				return 1;
			}
			],
			[AC_MSG_RESULT([yes (found $PILOT_LINK_VERSION)])],
			[AC_MSG_ERROR([pilot-link >= $1 required])],
			[AC_MSG_WARN([No action taken for crosscompile])]
		)
		CFLAGS="$CFLAGS_save"
	fi

	unset piversion_include
	unset pi_verion
	unset pi_major
	unset pi_minor
	unset pi_patch
	unset incdir
	unset pl_mi
	unset pl_ma
	unset pl_ve
])

AC_DEFUN([PILOT_LINK_CHECK],[
	PILOT_LINK_HOOK($1,[],nofailure)
])

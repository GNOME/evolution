dnl
dnl GNOME_PILOT_HOOK(script if found, fail)
dnl if fail = "failure", abort if gnome-pilot not found
dnl

GNOME_PILOT_CFLAGS=
GNOME_PILOT_LIBS=
PISOCK_INCLUDEDIR=
PISOCK_LIBS=
PILOT_BINS=
PILOT_LIBS=

AC_SUBST(GNOME_PILOT_CFLAGS)
AC_SUBST(GNOME_PILOT_LIBS)
AC_SUBST(PISOCK_INCLUDEDIR)
AC_SUBST(PISOCK_LIBS)
AC_SUBST(PILOT_BINS)
AC_SUBST(PILOT_LIBS)

AC_DEFUN([GNOME_PILOT_HOOK],[
	AC_PATH_PROG(GNOME_PILOT_CONFIG,gnome-pilot-config,no)
	AC_CACHE_CHECK([for gnome-pilot environment],gnome_cv_pilot_found,[
		if test x$GNOME_PILOT_CONFIG = xno; then
			gnome_cv_pilot_found=no
		else
			gnome_cv_pilot_found=yes
		fi
	])
	AM_CONDITIONAL(HAVE_GNOME_PILOT,test x$gnome_cv_pilot_found = xyes)
	if test x$gnome_cv_pilot_found = xyes; then
		$1
		AC_CHECK_HEADER(pi-version.h, [PISOCK_INCLUDEDIR=""], [
		AC_CHECK_HEADER(libpisock/pi-version.h, [PISOCK_INCLUDEDIR="-I/usr/include/libpisock"])])
		AC_CHECK_LIB(pisock, pi_accept, [ PISOCK_LIBS="-lpisock" ])
		GNOME_PILOT_CFLAGS=`gnome-pilot-config --cflags client conduitmgmt`
		GNOME_PILOT_LIBS=`gnome-pilot-config --libs client conduitmgmt`
	else
		if test x$2 = xfailure; then
			AC_MSG_ERROR(Gnome-pilot not installed or installation problem)
		fi
	fi
])

AC_DEFUN([GNOME_PILOT_CHECK],[
	GNOME_PILOT_HOOK([],nofailure)
])

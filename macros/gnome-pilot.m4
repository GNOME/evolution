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

AC_DEFUN([PILOT_LINK_HOOK],[
	AC_ARG_WITH(pisock,
	[  --with-pisock          Specify prefix for pisock files],[
	if test x$withval = xyes; then
	    dnl Note that an empty true branch is not valid sh syntax.
	    ifelse([$1], [], :, [$1])
	else
	    PISOCK_INCLUDEDIR="-I$withval/include"
	    PISOCK_LIBDIR="-L$withval/lib"
	    PISOCK_LIBS="-lpisock"
	    AC_MSG_CHECKING("for existance of $withval/lib/libpisock.so")
	    if test -r $withval/lib/libpisock.so; then
		AC_MSG_RESULT("yes")
	    else
		AC_MSG_ERROR("Unable to find libpisock. Try ftp://ryeham.ee.ryerson.ca/pub/PalmOS/.")
	    fi
	    AC_SUBST(PISOCK_INCLUDEDIR)
	    AC_SUBST(PISOCK_LIBDIR)
	fi
	])

	if test x$PISOCK_INCLUDEDIR = x; then
	    AC_CHECK_HEADER(pi-version.h, [], [
	    AC_CHECK_HEADER(libpisock/pi-version.h, PISOCK_INCLUDEDIR="-I/usr/include/libpisock",
	    AC_MSG_ERROR("Unable to find pi-version.h")) ])
	fi
	
	AC_SUBST(PISOCK_INCLUDEDIR)
	
	if test x$PISOCK_LIBDIR = x; then
		AC_CHECK_LIB(pisock, pi_accept, [ PISOCK_LIBS=-lpisock ], 
			[ AC_MSG_ERROR("Unable to find libpisock. Try ftp://ryeham.ee.ryerson.ca/pub/PalmOS/.") ])
	fi

])

AC_DEFUN([PILOT_LINK_CHECK],[
	PILOT_LINK_HOOK([],nofailure)
])

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
		PILOT_LINK_CHECK
		GNOME_PILOT_CFLAGS=`gnome-pilot-config --cflags client conduitmgmt`
		GNOME_PILOT_LIBS=`gnome-pilot-config --libs client conduitmgmt`
		$1
	else
		if test x$2 = xfailure; then
			AC_MSG_ERROR(Gnome-pilot not installed or installation problem)
		fi
	fi
])

AC_DEFUN([GNOME_PILOT_CHECK],[
	GNOME_PILOT_HOOK([],nofailure)
])


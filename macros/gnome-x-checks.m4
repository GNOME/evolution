dnl GNOME_X_CHECKS
dnl
dnl Basic X11 related checks for X11.  At the end, the following will be
dnl defined/changed:
dnl   x_{includes,libraries} From AC_PATH_X
dnl   X_{CFLAGS,LIBS}	     From AC_PATH_XTRA
dnl   X_{PRE,EXTRA}_LIBS     - do -
dnl   x_libs		     Essentially $X_PRE_LIBS -lX11 -Xext $X_EXTRA_LIBS
dnl   CPPFLAGS		     Will include $X_CFLAGS
dnl   GNOME_HAVE_SM	     `true' or `false' depending on whether session
dnl                          management is available.  It is available if
dnl                          both -lSM and X11/SM/SMlib.h exist.  (Some
dnl                          Solaris boxes have the library but not the header)
dnl
dnl The following configure cache variables are defined (but not used):
dnl   gnome_cv_passdown_{x_libs,X_LIBS,X_CFLAGS}
dnl
AC_DEFUN([GNOME_X_CHECKS],
[
        AC_PATH_X
        AC_PATH_XTRA

        saved_ldflags="$LDFLAGS"
        LDFLAGS="$X_LDFLAGS $X_LIBS"

	dnl Hope that X_CFLAGS have only -I and -D.  Otherwise, we could
	dnl   test -z "$x_includes" || CPPFLAGS="$CPPFLAGS -I$x_includes"
	dnl
	dnl Use CPPFLAGS instead of CFLAGS because AC_CHECK_HEADERS uses
	dnl CPPFLAGS, not CFLAGS
        CPPFLAGS="$CPPFLAGS $X_CFLAGS"

        dnl Checks for libraries.
        AC_CHECK_LIB(X11, XOpenDisplay,
                x_libs="$X_PRE_LIBS -lX11",
                [AC_MSG_ERROR(No X11 installed)],
                $X_EXTRA_LIBS)
	AC_SUBST(x_libs)

        LDFLAGS="$saved_ldflags $X_LDFLAGS $X_LIBS $x_libs"

        AC_CHECK_LIB(Xext, XShmAttach,
                x_libs="$x_libs -lXext", ,
                $x_libs $X_EXTRA_LIBS)

	x_libs="$x_libs $X_EXTRA_LIBS"
	gnome_cv_passdown_x_libs="$x_libs"
	gnome_cv_passdown_X_LIBS="$X_LIBS"
	gnome_cv_passdown_X_CFLAGS="$X_CFLAGS"

        LDFLAGS="$saved_ldflags $X_LDFLAGS $X_LIBS"

	GNOME_HAVE_SM=true
	case "$x_libs" in
	 *-lSM*)
	    dnl Already found it.
	    ;;
	 *)
	    dnl Assume that if we have -lSM then we also have -lICE.
	    AC_CHECK_LIB(SM, SmcSaveYourselfDone,
	        [x_libs="$x_libs -lSM -lICE"],GNOME_HAVE_SM=false,
		$x_libs -lICE)
	    ;;
	esac

	if test "$GNOME_HAVE_SM" = true; then
	   AC_CHECK_HEADERS(X11/SM/SMlib.h,,GNOME_HAVE_SM=false)
	fi

	if test "$GNOME_HAVE_SM" = true; then
	   AC_DEFINE(HAVE_LIBSM)
	fi

        AC_CHECK_LIB(gtk, gdk_pixmap_unref,
                GTK_LIBS="-lgtk -lgdk -lglib -lm",
                [AC_MSG_ERROR(Can not find a Gtk 0.99.1, probably you have an older version?)],
                -lgdk -lglib $x_libs -lm)
	AC_SUBST(GTK_LIBS)

	gnome_cv_passdown_GTK_LIBS="$GTK_LIBS"

	XPM_LIBS=""
	AC_CHECK_LIB(Xpm, XpmFreeXpmImage, [XPM_LIBS="-lXpm"], , $x_libs)
	AC_SUBST(XPM_LIBS)

	AC_REQUIRE([GNOME_PTHREAD_CHECK])
        LDFLAGS="$saved_ldflags"

	AC_PROVIDE([GNOME_X_CHECKS])
])

AC_DEFUN([GNOME_X_CHECKS],
[
        AC_PATH_X
        AC_PATH_XTRA

        saved_cflags="$CFLAGS"
        saved_ldflags="$LDFLAGS"

        CFLAGS="$X_CFLAGS"
        LDFLAGS="$X_LDFLAGS $X_LIBS"

        dnl Checks for libraries.
        AC_CHECK_LIB(X11, XOpenDisplay,
                x_libs="$X_PRE_LIBS -lX11 $X_EXTRA_LIBS",
                [AC_MSG_ERROR(No X11 installed)],
                $X_EXTRA_LIBS)
	AC_SUBST(x_libs)

        LDFLAGS="$saved_ldflags $X_LDFLAGS $X_LIBS $x_libs"

        AC_CHECK_LIB(Xext, XShmAttach,
                x_libs="$x_libs -lXext", ,
                $x_libs)

	gnome_cv_passdown_x_libs="$x_libs"
	gnome_cv_passdown_X_LIBS="$X_LIBS"
	gnome_cv_passdown_X_CFLAGS="$X_CFLAGS"

        LDFLAGS="$saved_ldflags $X_LDFLAGS $X_LIBS"

	dnl Assume that if we have -lSM then we also have -lICE.
	case "$x_libs" in
	 *-lSM*)
	    # Already found it.
	    AC_DEFINE(HAVE_LIBSM)
	    ;;
	 *)
	    AC_CHECK_LIB(SM, SmcSaveYourselfDone,
	        [AC_DEFINE(HAVE_LIBSM)
	        x_libs="$x_libs -lSM -lICE"], ,
		$x_libs -lICE)
		AM_CONDITIONAL(ENABLE_GSM,
			test "x$ac_cv_lib_SM_SmcSaveYourselfDone" = "xyes")
	    ;;
	esac

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

        CFLAGS="$saved_cflags $X_CFLAGS"
        LDFLAGS="$saved_ldflags"
	AC_PROVIDE([GNOME_X_CHECKS])
])

AC_DEFUN([GNOME_CHECK_GUILE],
[
	AC_CHECK_LIB(qthreads,main,[
	QTTHREADS_LIB="-lqthreads"
	AC_CHECK_LIB(termcap,main,TERMCAP_LIB="-ltermcap")
	AC_CHECK_LIB(readline,main,READLINE_LIB="-lreadline")
	],[
	AC_CHECK_LIB(qt, qt_null, QTTHREADS_LIB="-lqt")
	],$LIBS)
	AC_SUBST(TERMCAP_LIB)
	AC_SUBST(READLINE_LIB)
	AC_SUBST(QTTHREADS_LIB)

	AC_CHECK_LIB(guile, scm_boot_guile,[
	    GUILE_LIBS="-lguile"
	    ac_cv_guile_found=yes
	],[
	    AC_MSG_WARN(Can not find Guile 1.2 on the system)
	    ac_cv_guile_found=no
	], $QTTHREADS_LIB $LIBS)
	AC_SUBST(GUILE_LIBS)
	AM_CONDITIONAL(GUILE, test x$ac_cv_guile_found = xyes)
])

dnl
dnl GNOME_GNORBA_HOOK (script-if-gnorba-found, failflag)
dnl
dnl if failflag is "failure" it aborts if gnorba is not found.
dnl

AC_DEFUN([GNOME_GNORBA_HOOK],[
	GNOME_ORBIT_HOOK([],$2)
	AC_MSG_CHECKING(for gnorba libraries)
	GNORBA_CFLAGS=
	GNORBA_LIBS=
	if test -n "$ORBIT_LIBS"; then
		$1
		GNORBA_CFLAGS="`gnome-config --cflags gnorba gnomeui`"
		GNORBA_LIBS="`gnome-config --libs gnorba gnomeui`"
	fi
	if test -n "$GNORBA_LIBS"; then
		AC_SUBST(GNORBA_CFLAGS)
		AC_SUBST(GNORBA_LIBS)
		AC_MSG_RESULT(yes)
	else
	    	if test x$2 = xfailure; then
			AC_MSG_ERROR(Could not find gnorba libraries)
	    	fi
		AC_MSG_RESULT(no)
	fi
])

AC_DEFUN([GNOME_GNORBA_CHECK], [
	GNOME_GNORBA_HOOK([],failure)
])

dnl
dnl GNOME_ORBIT_HOOK (script-if-orbit-found, failflat)
dnl
dnl if failflag is "failure" it aborts if orbit is not found.
dnl

AC_DEFUN([GNOME_ORBIT_HOOK],[
	AC_PATH_PROG(ORBIT_CONFIG,orbit-config,no)
	if test x$ORBIT_CONFIG = xno; then
	    	if test x$2 = failure; then	
			AC_MSG_ERROR(Could not find orbit-config)
	    	fi
	else
		$1
		AC_DEFINE(HAVE_ORBIT)
	fi
])

AC_DEFUN([GNOME_ORBIT_CHECK], [
	GNOME_ORBIT_HOOK([],failure)
])

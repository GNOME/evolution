dnl
dnl GNOME_CONDUIT_HOOK (script-if-xml-found, failflag)
dnl
dnl If failflag is "failure", script aborts due to lack of XML
dnl 
dnl Check for availability of the Pilot conduit framework
dnl

AC_DEFUN([CONDUIT_LIBS_CHECK], [
	AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
	if test "$GNOME_CONFIG" = no; then
		if test x$2 = xfailure; then
			AC_MSG_ERROR(Could not find gnome-config)
		fi
	fi

	AC_MSG_CHECKING(for Palm conduit libs)
	if gnome-config --libs conduit > /dev/null 2>&1; then
		GNOME_CONDUIT_LIBS=`gnome-config --libs conduit`
		GNOME_CONDUIT_INCLUDEDIR=`gnome-config --cflags conduit`
		HAVE_GNOME_CONDUIT=yes
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi

	AC_SUBST(HAVE_GNOME_CONDUIT)
	AC_SUBST(GNOME_CONDUIT_LIBS)
	AC_SUBST(GNOME_CONDUIT_INCLUDEDIR)
])

AC_DEFUN([GNOME_CONDUIT_HOOK], [
	AC_ARG_ENABLE(conduits,
		[ --disable-conduits	disable Palm conduits ], [
		if test x$enableval = xno; then
			# do nothing
			true
		else
			# try to find conduit libs. error if not found.
			CONDUIT_LIBS_CHECK
			if test x$HAVE_GNOME_CONDUIT = x; then
				AC_MSG_ERROR(Palm conduit libraries not found)
			fi
		fi ], [ 
		CONDUIT_LIBS_CHECK ])
])
		
AC_DEFUN([GNOME_CONDUIT_CHECK], [
	GNOME_CONDUIT_HOOK([],failure)
])

dnl
dnl GNOME_PRINT_HOOK (script-if-gnome-print-found, failflag)
dnl
dnl if failflag is "failure" it aborts if gnome-print is not found
dnl

AC_DEFUN([GNOME_PRINT_HOOK],[
	AC_CHECK_LIB(gnomeprint, gnome_print_show, [
		$1
		AC_SUBST(GNOME_PRINT_LIB)
	], [
		if test x$2 = xfailure; then
			AC_MSG_ERROR(Could not link sample gnome-print program)
		fi
	], `gnome-config --libs print`)

	AC_MSG_CHECKING([for gnome-print headers])
	AC_TRY_CPP([#include <libgnomeprint/gnome-print.h>
		    #include <libgnomeprint/gnome-printer.h>
		    #include <libgnomeprint/gnome-font.h>
		    #include <libgnomeprint/gnome-printer-dialog.h>
		    #include <libgnomeprint/gnome-text.h>],
		    gnome_print_ok=yes, gnome_print_ok=no)

	AC_MSG_RESULT($gnome_print_ok)

	if test x"$gnome_print_ok" = xno; then
		AC_MSG_ERROR(Could not find gnome-print headers)
	fi
])

AC_DEFUN([GNOME_PRINT_CHECK], [
	GNOME_PRINT_HOOK([],failure)
])

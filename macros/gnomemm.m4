
dnl AM_PATH_GNOMEMM([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]])
dnl Test to see if gnomemm is installed, and define GNOMEMM_CFLAGS, LIBS
dnl
AC_DEFUN(AM_PATH_GNOMEMM,
[dnl
dnl Get the cflags and libraries from the gnome-config gnomemm script
dnl
dnl Ensure gnome-config is available...
AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
AC_MSG_CHECKING(for GNOME-- library)
if test -z "`gnome-config gnomemm --cflags`"; then
  AC_MSG_RESULT(no)
else
  AC_MSG_RESULT(yes)
  GNOMEMM_CFLAGS=`$GNOME_CONFIG gnomemm --cflags`
  GNOMEMM_LIBS=`$GNOME_CONFIG gnomemm --libs`
fi
AC_SUBST(GNOMEMM_CFLAGS)
AC_SUBST(GNOMEMM_LIBS)
])
dnl libsupport stuff
dnl GNOME_SUPPORT_CHECKS
AC_DEFUN([GNOME_SUPPORT_CHECKS],[
  AC_CHECK_FUNCS(getopt_long,,LIBOBJS="$LIBOBJS getopt.o getopt1.o")
  AC_REPLACE_FUNCS(strtok_r)
  AC_SUBST(LIBOBJS)
])

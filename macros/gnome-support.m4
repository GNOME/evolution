dnl libsupport stuff
dnl GNOME_SUPPORT_CHECKS
AC_DEFUN([GNOME_SUPPORT_CHECKS],[
  AC_CHECK_FUNCS(getopt_long,,LIBOBJS="$LIBOBJS getopt.o getopt1.o")
  AC_CHECK_FUNCS(argp_parse,,LIBOBJS="$LIBOBJS argp-ba.o argp-eexst.o argp-fmtstream.o argp-fs-xinl.o argp-help.o argp-parse.o argp-pv.o argp-xinl.o")
  AC_TRY_LINK([#include <errno.h>],[
    char *foo = program_invocation_short_name], [
    AC_DEFINE(HAVE_PROGRAM_INVOCATION_SHORT_NAME)])
  AC_TRY_LINK([#include <errno.h>],[
    char *foo = program_invocation_name], [
    AC_DEFINE(HAVE_PROGRAM_INVOCATION_NAME)])
  AC_REPLACE_FUNCS(strtok_r strcasecmp strndup strnlen)
  AC_SUBST(LIBOBJS)
])

dnl GNOME_SUPPORT_CHECKS
dnl    Check for various support functions needed by the standard
dnl    Gnome libraries.  Sets LIBOBJS, might define some macros,
dnl    and will set the need_gnome_support shell variable to "yes"
dnl    or "no".  Also sets up the Automake BUILD_GNOME_SUPPORT
dnl    conditional.  This should only be used when building the Gnome
dnl    libs; Gnome clients should not need this macro.
AC_DEFUN([GNOME_SUPPORT_CHECKS],[
  need_gnome_support=no
  save_LIBOBJS="$LIBOBJS"
  LIBOBJS=

  AC_CHECK_FUNCS(getopt_long,,LIBOBJS="$LIBOBJS getopt.o getopt1.o")

  # We check for argp_domain because we use it, and it appears only in 
  # very recent versions of the argp library.
  AC_CACHE_CHECK([for working argp code], gnome_cv_argp, [
    AC_TRY_COMPILE([#include <argp.h>], [
        struct argp foo;
	extern char *foo2;
	foo.argp_domain = foo2;],
      gnome_cv_argp=yes,
      gnome_cv_argp=no)

    # It is possible to have the argp headers installed but not have
    # the code in the C library.  At least, there have been reports of
    # this happening.  So we check for this case explicitly.
    if test "$gnome_cv_argp" = yes; then
       AC_CHECK_FUNC(argp_parse,,gnome_cv_argp=no)
    fi])

  if test "$gnome_cv_argp" = no; then
     LIBOBJS="$LIBOBJS argp-ba.o argp-eexst.o argp-fmtstream.o argp-fs-xinl.o argp-help.o argp-parse.o argp-pv.o argp-pvh.o argp-xinl.o"
  fi

  # This header enables some optimizations inside argp.  
  AC_CHECK_HEADERS(linewrap.h)

  AC_CACHE_CHECK([for program_invocation_short_name], gnome_cv_short_name, [
    AC_TRY_LINK([#include <errno.h>],[
      char *foo = program_invocation_short_name],
      gnome_cv_short_name=yes, gnome_cv_short_name=no)])
  if test "$gnome_cv_short_name" = yes; then
     AC_DEFINE(HAVE_PROGRAM_INVOCATION_SHORT_NAME)
  fi

  AC_CACHE_CHECK([for program_invocation_name], gnome_cv_invocation_name, [
    AC_TRY_LINK([#include <errno.h>],[
      char *foo = program_invocation_name],
      gnome_cv_invocation_name=yes, gnome_cv_invocation_name=no)])
  if test "$gnome_cv_invocation_name" = yes; then
     AC_DEFINE(HAVE_PROGRAM_INVOCATION_NAME)
  fi

  AC_CHECK_FUNCS(vsnprintf,,[
    AC_CHECK_FUNCS(__vsnprintf,
      LIBOBJS="$LIBOBJS easy-vsnprintf.o",
      LIBOBJS="$LIBOBJS vsnprintf.o")])

  AC_REPLACE_FUNCS(strtok_r strcasecmp strndup strnlen vasprintf)

  if test "$LIBOBJS" != ""; then
     need_gnome_support=yes
  fi
  # Turn our LIBOBJS into libtool objects.  This is gross, but it
  # requires changes to autoconf before it goes away.
  LTLIBOBJS=`echo "$LIBOBJS" | sed 's/\.o/.lo/g'`
  AC_SUBST(LTLIBOBJS)

  LIBOBJS="$save_LIBOBJS"
  AM_CONDITIONAL(BUILD_GNOME_SUPPORT, test "$need_gnome_support" = yes)
])

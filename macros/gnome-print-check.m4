# Configure paths for GNOME-PRINT
# Chris Lahey	99-2-5
# stolen from Manish Singh again
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_GNOME_PRINT([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for GNOME-PRINT, and define GNOME_PRINT_CFLAGS and GNOME_PRINT_LIBS
dnl
AC_DEFUN([AM_PATH_GNOME_PRINT],
[
  min_version=ifelse([$1],,0.21,$1)

  awk_alchemy=$'BEGIN {FS=".";} {print $\61 * 1000 + $\62;}'
  gnome_print_ok=""

  AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
  if test "$GNOME_CONFIG" = "no" ; then
    AC_MSG_RESULT(gnome-config is missing, check your gnome installation)
  else
    AC_MSG_CHECKING(for GNOME-PRINT - version >= $min_version)
    if `$GNOME_CONFIG --libs print > /dev/null 2>&1`; then
      gnome_print_version=$($GNOME_CONFIG --modversion print | sed -e 's/gnome-print-//' -e 's/cvs$//' | awk "$awk_alchemy")
      requested_version=`echo "$min_version" | awk "$awk_alchemy"`
      if test "$gnome_print_version" -ge "$requested_version"; then
        AC_MSG_RESULT(found)
        gnome_print_ok="yes"
      else
        AC_MSG_RESULT(not found)
      fi
    else
      AC_MSG_RESULT(gnome-print not installed)
    fi
  fi

  if test "x$gnome_print_ok" != "x" ; then
    GNOME_PRINT_CFLAGS=`$GNOME_CONFIG --cflags print`
    GNOME_PRINT_LIBS=`$GNOME_CONFIG --libs print`
    ifelse([$2], , :, [$2])
  else
     GNOME_PRINT_CFLAGS=""
     GNOME_PRINT_LIBS=""
     ifelse([$3], , :, [$3])
  fi

  AC_SUBST(GNOME_PRINT_CFLAGS)
  AC_SUBST(GNOME_PRINT_LIBS)
])

AC_DEFUN([GNOME_PRINT_CHECK], [
	AM_PATH_GNOME_PRINT($1,,[AC_MSG_ERROR(GNOME-PRINT not found or wrong version)])
])

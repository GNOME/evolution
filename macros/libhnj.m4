# Configure paths for LIBHNJ
# Chris Lahey	99-2-5
# stolen from Manish Singh again
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_LIBHNJ([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for LIBHNJ, and define LIBHNJ_CFLAGS and LIBHNJ_LIBS
dnl
AC_DEFUN(AM_PATH_LIBHNJ,
[dnl 
dnl Get the cflags and libraries from the libhnj-config script
dnl
AC_ARG_WITH(libhnj-prefix,[  --with-libhnj-prefix=PFX   Prefix where LIBHNJ is installed (optional)],
            libhnj_prefix="$withval", libhnj_prefix="")
AC_ARG_WITH(libhnj-exec-prefix,[  --with-libhnj-exec-prefix=PFX Exec prefix where LIBHNJ is installed (optional)],
            libhnj_exec_prefix="$withval", libhnj_exec_prefix="")
AC_ARG_ENABLE(libhnjtest, [  --disable-libhnjtest       Do not try to compile and run a test LIBHNJ program],
		    , enable_libhnjtest=yes)

  if test x$libhnj_exec_prefix != x ; then
     libhnj_args="$libhnj_args --exec-prefix=$libhnj_exec_prefix"
     if test x${LIBHNJ_CONFIG+set} != xset ; then
        LIBHNJ_CONFIG=$libhnj_exec_prefix/bin/libhnj-config
     fi
  fi
  if test x$libhnj_prefix != x ; then
     libhnj_args="$libhnj_args --prefix=$libhnj_prefix"
     if test x${LIBHNJ_CONFIG+set} != xset ; then
        LIBHNJ_CONFIG=$libhnj_prefix/bin/libhnj-config
     fi
  fi

  AC_PATH_PROG(LIBHNJ_CONFIG, libhnj-config, no)
  min_libhnj_version=ifelse([$1], ,0.1.0,$1)
  AC_MSG_CHECKING(for LIBHNJ - version >= $min_libhnj_version)
  no_libhnj=""
  if test "$LIBHNJ_CONFIG" = "no" ; then
    no_libhnj=yes
  else
    LIBHNJ_CFLAGS=`$LIBHNJ_CONFIG $libhnjconf_args --cflags`
    LIBHNJ_LIBS=`$LIBHNJ_CONFIG $libhnjconf_args --libs`

    libhnj_major_version=`$LIBHNJ_CONFIG $libhnj_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    libhnj_minor_version=`$LIBHNJ_CONFIG $libhnj_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    libhnj_micro_version=`$LIBHNJ_CONFIG $libhnj_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_libhnjtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $LIBHNJ_CFLAGS"
      LIBS="$LIBS $LIBHNJ_LIBS"
dnl
dnl Now check if the installed LIBHNJ is sufficiently new. (Also sanity
dnl checks the results of libhnj-config to some extent
dnl
      rm -f conf.libhnjtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libhnj/just.h>

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.libhnjtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_libhnj_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_libhnj_version");
     exit(1);
   }

   if (($libhnj_major_version > major) ||
      (($libhnj_major_version == major) && ($libhnj_minor_version > minor)) ||
      (($libhnj_major_version == major) && ($libhnj_minor_version == minor) && ($libhnj_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'libhnj-config --version' returned %d.%d.%d, but the minimum version\n", $libhnj_major_version, $libhnj_minor_version, $libhnj_micro_version);
      printf("*** of LIBHNJ required is %d.%d.%d. If libhnj-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If libhnj-config was wrong, set the environment variable LIBHNJ_CONFIG\n");
      printf("*** to point to the correct copy of libhnj-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_libhnj=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_libhnj" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$LIBHNJ_CONFIG" = "no" ; then
       echo "*** The libhnj-config script installed by LIBHNJ could not be found"
       echo "*** If LIBHNJ was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the LIBHNJ_CONFIG environment variable to the"
       echo "*** full path to libhnj-config."
     else
       if test -f conf.libhnjtest ; then
        :
       else
          echo "*** Could not run LIBHNJ test program, checking why..."
          CFLAGS="$CFLAGS $LIBHNJ_CFLAGS"
          LIBS="$LIBS $LIBHNJ_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <libhnj/just.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding LIBHNJ or finding the wrong"
          echo "*** version of LIBHNJ. If it is not finding LIBHNJ, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means LIBHNJ was incorrectly installed"
          echo "*** or that you have moved LIBHNJ since it was installed. In the latter case, you"
          echo "*** may want to edit the libhnj-config script: $LIBHNJ_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     LIBHNJ_CFLAGS=""
     LIBHNJ_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(LIBHNJ_CFLAGS)
  AC_SUBST(LIBHNJ_LIBS)
  rm -f conf.libhnjtest
])

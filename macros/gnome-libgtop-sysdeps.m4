dnl This file is intended for use both internally in libgtop and in every program
dnl that wants to use it.
dnl
dnl It defines the following variables:
dnl
dnl * 'libgtop_sysdeps_dir'    - sysdeps dir for libgtop.
dnl * 'libgtop_use_machine_h'  - some of system dependend parts of libgtop provide
dnl                              their own header file. In this case we need to
dnl                              define 'HAVE_GLIBTOP_MACHINE_H'.
dnl * 'libgtop_need_server'    - is the server really needed? Defines 'NEED_LIBGTOP'
dnl                              if true; defines conditional 'NEED_LIBGTOP'.

AC_DEFUN([GNOME_LIBGTOP_SYSDEPS],[
	AC_REQUIRE([AC_LC_CANONICAL_HOST])

	AC_SUBST(libgtop_sysdeps_dir)
	AC_SUBST(libgtop_use_machine_h)
	AC_SUBST(libgtop_need_server)

	AC_MSG_CHECKING(for libgtop sysdeps directory)

	AC_ARG_WITH(sysdeps,
	[  --with-sysdeps=dir      which sysdeps directory should be used [default=auto]],
	[if test "x$withval" = "xyes" ; then
	   ac_cv_sysdeps_dir=yes
	 elif test "x$withval" = "xauto" ; then
	   ac_cv_sysdeps_dir=yes
	 else
	   ac_cv_sysdeps_dir=$withval
	 fi],[ac_cv_sysdeps_dir=yes])

	if test "x$ac_cv_sysdeps_dir" = "xyes" ; then
	  case "$host_os" in
	  linux*)
	    libgtop_sysdeps_dir=linux
	    libgtop_use_machine_h=no
	    libgtop_need_server=no
	    ;;
	  sunos4*)
	    libgtop_sysdeps_dir=sun4
	    libgtop_use_machine_h=no
	    libgtop_need_server=yes
	    ;;
	  osf*)
	    libgtop_sysdeps_dir=osf1
	    libgtop_use_machine_h=yes
	    libgtop_need_server=yes
	    ;;
	  *)
	    libgtop_sysdeps_dir=stub
	    libgtop_use_machine_h=no
	    libgtop_need_server=no
	    ;;
	  esac
	else
	  libgtop_sysdeps_dir=stub
	  libgtop_use_machine_h=no
	  libgtop_need_server=no
	fi

	AC_MSG_RESULT($libgtop_sysdeps_dir)

	AC_MSG_CHECKING(for machine.h in libgtop sysdeps dir)
	AC_MSG_RESULT($libgtop_use_machine_h)

	AC_MSG_CHECKING(whether we need libgtop)
	AC_MSG_RESULT($libgtop_need_server)

	if test x$libgtop_need_server = xyes ; then
	  AC_DEFINE(NEED_LIBGTOP)
	fi

	if test x$libgtop_use_machine_h = xyes ; then
	  AC_DEFINE(HAVE_GLIBTOP_MACHINE_H)
	fi

	AM_CONDITIONAL(NEED_LIBGTOP, test x$libgtop_need_server = xyes)
])


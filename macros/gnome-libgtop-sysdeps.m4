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
	AC_REQUIRE([AC_CANONICAL_HOST])

	AC_SUBST(libgtop_sysdeps_dir)
	AC_SUBST(libgtop_use_machine_h)
	AC_SUBST(libgtop_need_server)

	AC_ARG_WITH(linux-table,
	[  --with-linux-table      Use the table () function from Martin Baulig],[
	linux_table="$withval"],[linux_table=auto])

	AC_MSG_CHECKING(for table function in Linux Kernel)

	if test $linux_table = yes ; then
	  AC_CHECK_HEADER(linux/table.h, linux_table=yes, linux_table=no)
	elif test $linux_table = auto ; then
	  AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <linux/unistd.h>
#include <linux/table.h>

#include <syscall.h>

static inline _syscall3 (int, table, int, type, union table *, tbl, const void *, param);

int
main (void)
{
	union table tbl;
	int ret;

	ret = table (TABLE_VERSION, NULL, NULL);

	if (ret == -1)
		exit (-errno);

	exit (ret < 1 ? ret : 0);
}
], linux_table=yes, linux_table=no, linux_table=no)
	fi

	AC_MSG_RESULT($linux_table)

	if test $linux_table = yes ; then
	  AC_DEFINE(HAVE_LINUX_TABLE)
	fi

	AM_CONDITIONAL(LINUX_TABLE, test $linux_table = yes)

	AC_MSG_CHECKING(for libgtop sysdeps directory)

	case "$host_os" in
	linux*)
	  if test x$linux_table = xyes ; then
	    libgtop_sysdeps_dir=kernel
	    libgtop_use_machine_h=no
	  else
	    libgtop_sysdeps_dir=linux
	    libgtop_use_machine_h=yes
	  fi
	  libgtop_need_server=no
	  ;;
	sunos4*)
	  libgtop_sysdeps_dir=sun4
	  libgtop_use_machine_h=yes
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


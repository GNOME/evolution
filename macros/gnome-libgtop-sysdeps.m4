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

	AC_ARG_WITH(libgtop-examples,
	[  --with-libgtop-examples Build the libgtop examples (default=no)],[
	build_examples="$withval"], [build_examples=no])

	AM_CONDITIONAL(EXAMPLES, test x"$build_examples" = xyes)


	AC_ARG_WITH(libgtop-smp,
	[  --with-libgtop-smp      Enable SMP support (default-auto)],[
	libgtop_smp="$withval"],[libgtop_smp=auto])

	if test $libgtop_smp = auto ; then
	  AC_MSG_CHECKING(whether to enable SMP support)
	  AC_TRY_RUN([
#include <sys/utsname.h>
#include <string.h>

int
main (void)
{
	struct utsname name;

	if (uname (&name)) exit (1);

	exit (strstr (name.version, "SMP") ? 0 : 1);
}
], libgtop_smp=yes, libgtop_smp=no, libgtop_smp=no)
	  AC_MSG_RESULT($libgtop_smp)
	fi

	if test $libgtop_smp = yes ; then
	  AC_DEFINE(HAVE_LIBGTOP_SMP)
	fi

	AM_CONDITIONAL(LIBGTOP_SMP, test $libgtop_smp = yes)

	AC_MSG_CHECKING(for libgtop sysdeps directory)

	case "$host_os" in
	linux*)
	  if test x$linux_table = xyes ; then
	    libgtop_sysdeps_dir=kernel
	    libgtop_use_machine_h=no
	  else
	    libgtop_sysdeps_dir=linux
	    libgtop_use_machine_h=no
	    libgtop_have_sysinfo=yes
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
	freebsd*)
	  libgtop_sysdeps_dir=freebsd
	  libgtop_use_machine_h=yes
	  libgtop_need_server=yes
	  libgtop_postinstall='chgrp kmem $(bindir)/libgtop_server && chmod 2755 $(bindir)/libgtop_server'
	  ;;
	*)
	  libgtop_sysdeps_dir=stub
	  libgtop_use_machine_h=no
	  libgtop_need_server=no
	  ;;
	esac

	test -z "$libgtop_postinstall" && libgtop_postinstall=:

	AC_MSG_RESULT($libgtop_sysdeps_dir)

	AC_SUBST(libgtop_sysdeps_dir)
	AC_SUBST(libgtop_postinstall)
	AC_SUBST(libgtop_have_sysinfo)

	case "$host_os" in
	*bsd*)
	  AC_CHECK_HEADERS(net/if_var.h)
	  AC_MSG_CHECKING([for I4B])
	  AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>

#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif

#include <net/netisr.h>
#include <net/route.h>

#ifdef __FreeBSD__
#include <net/if_sppp.h>
#else
#include <i4b/sppp/if_sppp.h>
#endif
],[
	size_t size = sizeof (struct sppp);
], have_i4b=yes, have_i4b=no)
	  AC_MSG_RESULT($have_i4b)
	  if test x$have_i4b = xyes; then
	    AC_DEFINE(HAVE_I4B)
	    AC_MSG_CHECKING([for I4B accounting])
	    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>

#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif

#include <net/netisr.h>
#include <net/route.h>

#ifdef __FreeBSD__
#include <net/if_sppp.h>
#else
#include <i4b/sppp/if_sppp.h>
#endif

#include <machine/i4b_acct.h>
],[
	size_t size = sizeof (struct i4bisppp_softc);
], have_i4b_acct=yes, have_i4b_acct=no)
	    AC_MSG_RESULT($have_i4b_acct)
	    if test x$have_i4b_acct = xyes ; then
	      AC_DEFINE(HAVE_I4B_ACCT)
	    else
	      AC_WARN([
*** I4B accounting disabled - you won't get any PPP statistics.
*** Read "misc/i4b_acct.txt" in the LibGTop source directory
*** to see how to enable it.])
	    fi
	  fi
	  ;;
	linux*)
	  AC_ARG_WITH(linux-table,
	  [  --with-linux-table      Use the table () function from Martin Baulig],[
	  linux_table="$withval"],[linux_table=auto])
	  if test $linux_table = yes ; then
	    AC_CHECK_HEADER(linux/table.h, linux_table=yes, linux_table=no)
	  elif test $linux_table = auto ; then
	    AC_MSG_CHECKING(for table function in Linux Kernel)
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
	    AC_MSG_RESULT($linux_table)
	  fi
	  if test $linux_table = yes ; then
	    AC_DEFINE(HAVE_LINUX_TABLE)
	  fi
	  AM_CONDITIONAL(LINUX_TABLE, test $linux_table = yes)
	  ;;
	esac

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


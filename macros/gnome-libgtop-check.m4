dnl
dnl GNOME_LIBGTOP_TYPES
dnl
dnl some typechecks for libgtop.
dnl

AC_DEFUN([GNOME_LIBGTOP_TYPES],
[
	AC_CHECK_TYPE(u_int64_t, unsigned long long int)
	AC_CHECK_TYPE(int64_t, long long int)
])

dnl
dnl GNOME_LIBGTOP_HOOK (script-if-libgtop-enabled, failflag)
dnl
dnl if failflag is "fail" then GNOME_LIBGTOP_HOOK will abort if gtopConf.sh
dnl is not found. 
dnl

AC_DEFUN([GNOME_LIBGTOP_HOOK],
[	
	AC_REQUIRE([GNOME_LIBGTOP_TYPES])

	AC_SUBST(LIBGTOP_LIBDIR)
	AC_SUBST(LIBGTOP_INCLUDEDIR)
	AC_SUBST(LIBGTOP_EXTRA_LIBS)
	AC_SUBST(LIBGTOP_LIBS)
	AC_SUBST(LIBGTOP_INCS)
	AC_SUBST(LIBGTOP_NAMES_LIBS)
	AC_SUBST(LIBGTOP_NAMES_INCS)
	AC_SUBST(LIBGTOP_GUILE_INCS)
	AC_SUBST(LIBGTOP_GUILE_LIBS)
	AC_SUBST(LIBGTOP_GUILE_NAMES_INCS)
	AC_SUBST(LIBGTOP_GUILE_NAMES_LIBS)
	AC_SUBST(LIBGTOP_MINOR_VERSION)
	AC_SUBST(LIBGTOP_MAJOR_VERSION)
	AC_SUBST(LIBGTOP_INTERFACE_AGE)
	AC_SUBST(LIBGTOP_BINARY_AGE)
	AC_SUBST(LIBGTOP_VERSION)
	AC_SUBST(LIBGTOP_BINDIR)
	AC_SUBST(LIBGTOP_SERVER)

	dnl Get the cflags and libraries from the libgtop-config script
	dnl
	AC_ARG_WITH(libgtop,
	[  --with-libgtop=PFX      Prefix where LIBGTOP is installed (optional)],
	libgtop_config_prefix="$withval", libgtop_config_prefix="")
	AC_ARG_WITH(libgtop-exec,
	[  --with-libgtop-exec=PFX Exec prefix where LIBGTOP is installed (optional)],
	libgtop_config_exec_prefix="$withval", libgtop_config_exec_prefix="")

	if test x$libgtop_config_exec_prefix != x ; then
	  libgtop_config_args="$libgtop_config_args --exec-prefix=$libgtop_config_exec_prefix"
	  if test x${LIBGTOP_CONFIG+set} != xset ; then
	    LIBGTOP_CONFIG=$libgtop_config_exec_prefix/bin/libgtop-config
	  fi
	fi
	if test x$libgtop_config_prefix != x ; then
	  libgtop_config_args="$libgtop_config_args --prefix=$libgtop_config_prefix"
	  if test x${LIBGTOP_CONFIG+set} != xset ; then
	    LIBGTOP_CONFIG=$libgtop_config_prefix/bin/libgtop-config
	  fi
	fi

	AC_PATH_PROG(LIBGTOP_CONFIG, libgtop-config, no)
	min_libgtop_version=ifelse([$1], ,0.25.1,$1)
	AC_MSG_CHECKING(for libgtop - version >= $min_libgtop_version)
	no_libgtop=""
	if test "$LIBGTOP_CONFIG" = "no" ; then
	  no_libgtop=yes
	else
	  configfile=`$LIBGTOP_CONFIG --config`
	  libgtop_version=`$LIBGTOP_CONFIG --version | sed -e 's,pre.*,,'`
	  test $libgtop_version \< $min_libgtop_version && no_libgtop=yes
	  . $configfile
	fi
	if test x$no_libgtop = x ; then
	  AC_DEFINE(HAVE_LIBGTOP)
	  AC_MSG_RESULT(yes)
	else
	  AC_MSG_RESULT(no)
	fi

	AM_CONDITIONAL(HAVE_LIBGTOP, test x$no_libgtop != xyes)
])

AC_DEFUN([GNOME_INIT_LIBGTOP],[
	GNOME_LIBGTOP_HOOK([],)
])

dnl EVO_PURIFY_SUPPORT
dnl Add --enable-purify. If the user turns it on, subst PURIFY and set
dnl the automake conditional ENABLE_PURIFY
#serial 0.1
AC_DEFUN([EVO_PURIFY_SUPPORT],
	[AC_ARG_ENABLE([purify],
		[AS_HELP_STRING([--enable-purify],
		[Enable support for building executables with Purify.])],
		[enable_purify=yes],[enable_purify=no])
	AC_PATH_PROG(PURIFY, purify, impure)
	AC_ARG_WITH([purify-options],
		[AS_HELP_STRING([--with-purify-options@<:@=OPTIONS@:>@],
		[Options passed to the purify command line (defaults to PURIFYOPTIONS variable).])])
	if test "x$with_purify_options" = "xno"; then
		with_purify_options="-always-use-cache-dir=yes -cache-dir=/gnome/lib/purify"
	fi
	if test "x$PURIFYOPTIONS" = "x"; then
		PURIFYOPTIONS=$with_purify_options
	fi
	AC_SUBST(PURIFY)
	AM_CONDITIONAL(ENABLE_PURIFY, [test x$enable_purify = xyes -a x$PURIFY != ximpure])
	PURIFY="$PURIFY $PURIFYOPTIONS"
])

dnl EVO_CHECK_LANGINFO(detail)
dnl Checks if the given langinfo detail is supported
AC_DEFUN([EVO_CHECK_LANGINFO],[
	AS_VAR_PUSHDEF([ac_cv_langinfo_detail],
	               [ac_cv_langinfo_]m4_tolower($1))

	AC_MSG_CHECKING([for nl_langinfo ($1)])

	AC_LANG_PUSH(C)

	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
			[[#include <langinfo.h>]],
			[[char *detail = nl_langinfo ($1);]])],
		[ac_cv_langinfo_detail=yes],
		[ac_cv_langinfo_detail=no])

	AC_LANG_POP(C)

	AS_VAR_IF([ac_cv_langinfo_detail], [yes],
	          [AC_DEFINE([HAVE_]m4_toupper($1), 1, [Have nl_langinfo ($1)])])

	AC_MSG_RESULT([$ac_cv_langinfo_detail])
	AS_VAR_POPDEF([ac_cv_langinfo_detail])
])

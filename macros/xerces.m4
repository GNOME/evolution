dnl Curses detection: Munged from curses.m4 which was munged from 
dnl Midnight Commander's configure.in
dnl
dnl What it does:
dnl =============
dnl
dnl - Determine which version of xerces is installed on the system
dnl - Do an AC_SUBST on the CURSES_INCLUDEDIR and CURSES_LIBS so that
dnl   @CURSES_INCLUDEDIR@ and @CURSES_LIBS@ will be available in
dnl   Makefile.in's

AC_DEFUN([AC_CHECK_XERCES],[
	has_xerces=false

	CFLAGS=${CFLAGS--O}

	AC_ARG_WITH(xerces,
	  [  --with-xerces=dir       Specify the Xerces directory],[
	  if test x$withval != x; then
		XERCES_INCLUDE_DIR="$withval/include"
		XERCES_LIB_DIR=$withval/lib
	  fi
	])

	AC_MSG_CHECKING(for xerces)
	if test x$XERCES_INCLUDE_DIR = x; then
		if test x$XERCESCROOT = x; then
			if test "x$prefix" != "xNONE"; then
				XERCESCROOT="$prefix"
			fi
		fi
		XERCES_INCLUDE_DIR=$XERCESCROOT/include
	fi

	if test x$XERCES_LIB_DIR = x; then
		XERCES_LIB_DIR=$XERCESCROOT/lib
	fi

	XERCES_VER=`ls $XERCES_LIB_DIR/libxerces*.so | 
		    perl macros/xerces-version.pl`

	if test "x$XERCES_VER" = "x0_0"; then
		AC_MSG_ERROR("You must have Xerces installed and set XERCESCROOT or use --with-xerces")
	else
		AC_MSG_RESULT(found)
	fi

	XERCES_LIBNAME=xerces-c
	XERCES_LIBRARY_NAMES=-l${XERCES_LIBNAME}${XERCES_VER}
	XERCES_LIBRARY_SEARCH_PATHS=-L${XERCES_LIB_DIR}
	XERCES_INCLUDE=-I${XERCES_INCLUDE_DIR}

	AC_SUBST(XERCES_LIBRARY_NAMES)
	AC_SUBST(XERCES_LIBNAME)
	AC_SUBST(XERCES_INCLUDE)
	AC_SUBST(XERCES_INCLUDE_DIR)
	AC_SUBST(XERCES_VER)
	AC_SUBST(XERCES_LIBRARY_SEARCH_PATHS)

	if test -f "${XERCES_INCLUDE_DIR}/util/XMLUniDefs.hpp"; then
		AC_DEFINE(HAVE_XMLUNIDEFS)
	fi
])



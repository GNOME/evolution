dnl Curses detection routines: Munged from Midnight commander's configure.in
dnl
dnl Called after AC_CHECK_CURSES after AC_PROG_CC
dnl
dnl Will adjust CURSES_INCLUDEDIR, CURSES_LIBS, and possibly CFLAGS
dnl Also do AC_SUBST on CURSES_INCLUDEDIR and CURSES_LIBS
dnl
dnl Add this to acconfig.h:
dnl
dnl /*=== Curses variables swiped from Midnight commander's config ===*/
dnl    
dnl /* Found some version of curses that we're going to use */
dnl #undef HAS_CURSES
dnl    
dnl /* Are you using other type of curses? */
dnl #undef OTHER_CURSES
dnl 
dnl /* Use SunOS SysV curses? */
dnl #undef SUNOS_CURSES
dnl 
dnl /* Use old BSD curses? */
dnl #undef USE_BSD_CURSES
dnl 
dnl /* Use SystemV curses? */
dnl #undef USE_SYSV_CURSES
dnl 
dnl /* Use Ncurses? */
dnl #undef USE_NCURSES
dnl 
dnl /* If you Curses does not have color define this one */
dnl #undef NO_COLOR_CURSES
dnl 
dnl /* Define if you want to turn on SCO-specific code */
dnl #undef SCO_FLAVOR
dnl 
dnl /* Version of ncurses */
dnl #undef NCURSES_970530


AC_DEFUN(AC_CHECK_CURSES,[
	search_ncurses=true
	screen_manager=""
	has_curses=false

	CFLAGS=${CFLAGS--O}

dnl	XCURSES=""
dnl	AC_SUBST(XCURSES)
	AC_SUBST(CURSES_LIBS)
	AC_SUBST(CURSES_INCLUDEDIR)

	AC_ARG_WITH(sco,
	  [  --with-sco              Use this to turn on SCO-specific code],[
	  if test x$withval = xyes; then
		AC_DEFINE(SCO_FLAVOR)
		CFLAGS="$CFLAGS -D_SVID3"
	  fi
	])

	AC_ARG_WITH(sunos-curses,
	  [  --with-sunos-curses     Used to force SunOS 4.x curses],[
	  if test x$withval = xyes; then
		AC_USE_SUNOS_CURSES
	  fi
	])

	AC_ARG_WITH(osf1-curses,
	  [  --with-osf1-curses      Used to force OSF/1 curses],[
	  if test x$withval = xyes; then
		AC_USE_OSF1_CURSES
	  fi
	])

	AC_ARG_WITH(vcurses,
	  [  --with-vcurses[=incdir] Used to force SysV curses],
	  if test x$withval != xyes; then
		CURSES_INCLUDEDIR="-I$withval"
	  fi
	  AC_USE_SYSV_CURSES
	)

	AC_ARG_WITH(ncurses,
	  [  --with-ncurses[=dir]  Compile with ncurses/locate base dir],
	  if test x$withval = xno ; then
		search_ncurses=false
	  elif test x$withval != xyes ; then
		CURSES_LIBS="$LIBS -L$withval/lib -lncurses"
		CURSES_INCLUDEDIR="-I$withval/include"
		search_ncurses=false
		screen_manager="ncurses"
		AC_DEFINE(USE_NCURSES)
		AC_DEFINE(HAS_CURSES)
		has_curses=true
	  fi
	)

	if $search_ncurses
	then
		AC_SEARCH_NCURSES()
	fi


])


AC_DEFUN(AC_USE_SUNOS_CURSES, [
	search_ncurses=false
	screen_manager="SunOS 4.x /usr/5include curses"
	AC_MSG_RESULT(Using SunOS 4.x /usr/5include curses)
	AC_DEFINE(SUNOS_CURSES)
	AC_DEFINE(HAS_CURSES)
	has_curses=true
	AC_DEFINE(NO_COLOR_CURSES)
	AC_DEFINE(USE_SYSV_CURSES)
	CURSES_INCLUDEDIR="-I/usr/5include"
dnl	XCURSES="xcurses.o"
	CURSES_LIBS="/usr/5lib/libcurses.a /usr/5lib/libtermcap.a"
	AC_MSG_RESULT(Please note that some screen refreshs may fail)
dnl	AC_WARN(Reconsider using Slang)
])

AC_DEFUN(AC_USE_OSF1_CURSES, [
       AC_MSG_RESULT(Using OSF1 curses)
       search_ncurses=false
       screen_manager="OSF1 curses"
       AC_DEFINE(HAS_CURSES)
       has_curses=true
       AC_DEFINE(NO_COLOR_CURSES)
       AC_DEFINE(USE_SYSV_CURSES)
dnl    XCURSES="xcurses.o"
       CURSES_LIBS="-lcurses"
])

AC_DEFUN(AC_USE_SYSV_CURSES, [
	AC_MSG_RESULT(Using SysV curses)
	AC_DEFINE(HAS_CURSES)
	has_curses=true
	AC_DEFINE(USE_SYSV_CURSES)
dnl	XCURSES=""
	search_ncurses=false
	screen_manager="SysV/curses"
	CURSES_LIBS="-lcurses"
])

dnl AC_ARG_WITH(bsd-curses,
dnl [--with-bsd-curses         Used to compile with bsd curses, not very fancy],
dnl 	search_ncurses=false
dnl	screen_manager="Ultrix/cursesX"
dnl	if test $system = ULTRIX
dnl	then
dnl	    THIS_CURSES=cursesX
dnl        else
dnl	    THIS_CURSES=curses
dnl	fi
dnl
dnl	CURSES_LIBS="-l$THIS_CURSES -ltermcap"
dnl	AC_DEFINE(HAS_CURSES)
dnl	has_curses=true
dnl	AC_DEFINE(USE_BSD_CURSES)
dnl	XCURSES="xcurses.o"
dnl	AC_MSG_RESULT(Please note that some screen refreshs may fail)
dnl	AC_WARN(Use of the bsdcurses extension has some)
dnl	AC_WARN(display/input problems.)
dnl	AC_WARN(Reconsider using xcurses)
dnl)

	
dnl
dnl Parameters: directory filename cureses_LIBS curses_INCLUDEDIR nicename
dnl
AC_DEFUN(AC_NCURSES, [
    if $search_ncurses
    then
        if test -f $1/$2
	then
	    AC_MSG_RESULT(Found ncurses on $1/$2)
 	    CURSES_LIBS="$3"
	    CURSES_INCLUDEDIR="$4"
	    search_ncurses=false
	    screen_manager=$5
            AC_DEFINE(HAS_CURSES)
            has_curses=true
	    AC_DEFINE(USE_NCURSES)
	fi
    fi
])

AC_DEFUN(AC_SEARCH_NCURSES, [
    AC_CHECKING("location of ncurses.h file")

    AC_NCURSES(/usr/include, ncurses.h, -lncurses,, "ncurses on /usr/include")
    AC_NCURSES(/usr/include/ncurses, ncurses.h, -lncurses, -I/usr/include/ncurses, "ncurses on /usr/include/ncurses")
    AC_NCURSES(/usr/local/include, ncurses.h, -L/usr/local/lib -lncurses, -I/usr/local/include, "ncurses on /usr/local")
    AC_NCURSES(/usr/local/include/ncurses, ncurses.h, -L/usr/local/lib -L/usr/local/lib/ncurses -lncurses, -I/usr/local/include/ncurses, "ncurses on /usr/local/include/ncurses")

    AC_NCURSES(/usr/local/include/ncurses, curses.h, -L/usr/local/lib -lncurses, -I/usr/local/include/ncurses -DRENAMED_NCURSES, "renamed ncurses on /usr/local/.../ncurses")

    AC_NCURSES(/usr/include/ncurses, curses.h, -lncurses, -I/usr/include/ncurses -DRENAMED_NCURSES, "renamed ncurses on /usr/include/ncurses")

    dnl
    dnl We couldn't find ncurses, try SysV curses
    dnl
    if $search_ncurses 
    then
        AC_EGREP_HEADER(init_color, /usr/include/curses.h,
	    AC_USE_SYSV_CURSES)
	AC_EGREP_CPP(USE_NCURSES,[
#include <curses.h>
#ifdef __NCURSES_H
#undef USE_NCURSES
USE_NCURSES
#endif
],[
	CURSES_INCLUDEDIR="$CURSES_INCLUDEDIR -DRENAMED_NCURSES"
        AC_DEFINE(HAS_CURSES)
	has_curses=true
        AC_DEFINE(USE_NCURSES)
        search_ncurses=false
        screen_manager="ncurses installed as curses"
])
    fi

    dnl
    dnl Try SunOS 4.x /usr/5{lib,include} ncurses
    dnl The flags SUNOS_CURSES, USE_BSD_CURSES and BUGGY_CURSES
    dnl should be replaced by a more fine grained selection routine
    dnl
    if $search_ncurses
    then
	if test -f /usr/5include/curses.h
	then
	    AC_USE_SUNOS_CURSES
        fi
    else
        # check for ncurses version, to properly ifdef mouse-fix
	AC_MSG_CHECKING(for ncurses version)
	ncurses_version=unknown
cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
#ifdef RENAMED_NCURSES
#include <curses.h>
#else
#include <ncurses.h>
#endif
#undef VERSION
VERSION:NCURSES_VERSION
EOF
        if (eval "$ac_cpp conftest.$ac_ext") 2>&AC_FD_CC |
  egrep "VERSION:" >conftest.out 2>&1; then
changequote(,)dnl
            ncurses_version=`cat conftest.out|sed -e 's/^[^"]*"//' -e 's/".*//'`
changequote([,])dnl
	fi
	rm -rf conftest*
        AC_MSG_RESULT($ncurses_version)
	case "$ncurses_version" in
changequote(,)dnl
	4.[01])
changequote([,])dnl
            AC_DEFINE(NCURSES_970530,2)
            ;;
	1.9.9g)
            AC_DEFINE(NCURSES_970530,1)
            ;;
	1*)
            AC_DEFINE(NCURSES_970530,0)
            ;;
	esac
    fi
])






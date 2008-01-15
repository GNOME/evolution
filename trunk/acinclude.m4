# evolution/acinclude.m4
# shared configure.in hacks between Evolution and Connector

# serial 1

# EVO_PURIFY_SUPPORT
# Add --enable-purify. If the user turns it on, subst PURIFY and set
# the automake conditional ENABLE_PURIFY
AC_DEFUN([EVO_PURIFY_SUPPORT], [
	AC_ARG_ENABLE([purify], 
		      AC_HELP_STRING([--enable-purify],
			       [Enable support for building executables with Purify.]),,[enable_purify=no])
	AC_PATH_PROG(PURIFY, purify, impure)
	AC_ARG_WITH([purify-options], 
		    AC_HELP_STRING([--with-purify-options=OPTIONS],
		    		   [Options passed to the purify command line (defaults to PURIFYOPTIONS variable).]))
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


# EVO_LDAP_CHECK(default)
# Add --with-openldap and --with-static-ldap options. --with-openldap
# defaults to the given value if not specified. If LDAP support is
# configured, HAVE_LDAP will be defined and the automake conditional
# ENABLE_LDAP will be set. LDAP_CFLAGS and LDAP_LIBS will be set
# appropriately.
AC_DEFUN([EVO_LDAP_CHECK], [
	default="$1"

	AC_ARG_WITH([openldap],
		    AC_HELP_STRING([--with-openldap],
		    		   [Enable LDAP support in evolution]))
	AC_ARG_WITH([static-ldap],
		    AC_HELP_STRING([--with-static-ldap],
		    		   [Link LDAP support statically into evolution]))
	AC_CACHE_CHECK([for OpenLDAP], ac_cv_with_openldap, ac_cv_with_openldap="${with_openldap:=$default}")
	case $ac_cv_with_openldap in
	no|"")
		with_openldap=no
		;;
	yes)
		with_openldap=/usr
		;;
	*)
		with_openldap=$ac_cv_with_openldap
		LDAP_CFLAGS="-I$ac_cv_with_openldap/include"
		LDAP_LDFLAGS="-L$ac_cv_with_openldap/lib"
		;;
	esac

	if test "$with_openldap" != no; then
		AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])

		case $with_static_ldap in
		no|"")
			with_static_ldap=no
			;;
		*)
			with_static_ldap=yes
			;;
		esac

		AC_CACHE_CHECK(if OpenLDAP is version 2.x, ac_cv_openldap_version2, [
			CPPFLAGS_save="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $LDAP_CFLAGS"
			AC_EGREP_CPP(yes, [
				#include "ldap.h"
				#if LDAP_VENDOR_VERSION > 20000
				yes
				#endif
			], ac_cv_openldap_version2=yes, ac_cv_openldap_version2=no)
			CPPFLAGS="$CPPFLAGS_save"
		])
		if test "$ac_cv_openldap_version2" = no; then
			AC_MSG_ERROR(evolution requires OpenLDAP version >= 2)
		fi

		AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="-lresolv")
		AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
		AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
		AC_CHECK_LIB(lber, ber_get_tag, [
			if test "$with_static_ldap" = "yes"; then
				LDAP_LIBS="$with_openldap/lib/liblber.a $LDAP_LIBS"

				# libldap might depend on OpenSSL... We need to pull
				# in the dependency libs explicitly here since we're
				# not using libtool for the configure test.
				if test -f $with_openldap/lib/libldap.la; then
					LDAP_LIBS="`. $with_openldap/lib/libldap.la; echo $dependency_libs` $LDAP_LIBS"
				fi
			else
				LDAP_LIBS="-llber $LDAP_LIBS"
			fi
			AC_CHECK_LIB(ldap, ldap_open, [
					if test $with_static_ldap = "yes"; then
						LDAP_LIBS="$with_openldap/lib/libldap.a $LDAP_LIBS"
					else
						LDAP_LIBS="-lldap $LDAP_LIBS"
					fi],
				LDAP_LIBS="", $LDAP_LDFLAGS $LDAP_LIBS)
			LDAP_LIBS="$LDAP_LDFLAGS $LDAP_LIBS"
		], LDAP_LIBS="", $LDAP_LDFLAGS $LDAP_LIBS)

		if test -z "$LDAP_LIBS"; then
			AC_MSG_ERROR(could not find OpenLDAP libraries)
		fi

		AC_SUBST(LDAP_CFLAGS)
		AC_SUBST(LDAP_LIBS)
	fi
	AM_CONDITIONAL(ENABLE_LDAP, test $with_openldap != no)
])

# EVO_SUNLDAP_CHECK
# Add --with-sunldap and --with-static-sunldap options. --with-sunldap
# defaults to the given value if not specified. If LDAP support is
# configured, HAVE_LDAP will be defined and the automake conditional +# ENABLE_LDAP will be set. LDAP_CFLAGS and LDAP_LIBS will be set
# appropriately, and --with-sunldap and --with-openldap is mutually exclusive.
AC_DEFUN([EVO_SUNLDAP_CHECK], [
        default="$1"

        AC_ARG_WITH([sunldap],
		    AC_HELP_STRING([--with-sunldap],
		    		   [Enable SunLDAP support in evolution]))
        AC_ARG_WITH([static-sunldap],
		    AC_HELP_STRING([--with-static-sunldap],
		    		   [Link SunLDAP support statically into evolution ]))
        AC_CACHE_CHECK([for SunLDAP], ac_cv_with_sunldap, ac_cv_with_sunldap="${with_sunldap:=$default}")
        case $ac_cv_with_sunldap in
        no|"")
                with_sunldap=no
                ;;
        yes)
                with_sunldap=/usr
                ;;
        *)
                with_sunldap=$ac_cv_with_sunldap
                LDAP_CFLAGS="-I$ac_cv_with_sunldap/include"
                LDAP_LDFLAGS="-L$ac_cv_with_sunldap/lib"
                ;;
        esac

        if test "$with_sunldap" != no; then
                AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])
                AC_DEFINE(SUNLDAP, 1, [Define if you use SunLDAP])

                case $with_static_sunldap in
                no|"")
                        with_static_sunldap=no
                        ;;
                *)
                       with_static_sunldap=yes
                        ;;
                esac

                AC_CACHE_CHECK(if SunLDAP is version 2.x, ac_cv_sunldap_version2, [
                        CPPFLAGS_save="$CPPFLAGS"
                        CPPFLAGS="$CPPFLAGS $LDAP_CFLAGS"
                        AC_EGREP_CPP(yes, [
                                #include "ldap.h"
                                #if LDAP_VENDOR_VERSION >= 500
                                yes
                                #endif
                        ], ac_cv_sunldap_version2=yes, ac_cv_sunldap_version2=no)
                        CPPFLAGS="$CPPFLAGS_save"
                ])
                if test "$ac_cv_sunldap_version2" = no; then
                       AC_MSG_ERROR(evolution requires SunLDAP version >= 2)
                fi

                AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="-lresolv")
                AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
                AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
                AC_CHECK_LIB(ldap, ldap_open, [
                        if test $with_static_sunldap = "yes"; then
                                LDAP_LIBS="$with_sunldap/lib/libldap.a $LDAP_LIBS"
                        else
                                LDAP_LIBS="-lldap $LDAP_LIBS"
                        fi
                        if test `uname -s` != "SunOS" ; then
                                AC_CHECK_LIB(lber, ber_get_tag, [
                                        if test "$with_static_sunldap" = "yes"; then
                                                LDAP_LIBS="$with_sunldap/lib/liblber.a $LDAP_LIBS"
                                                # libldap might depend on OpenSSL... We need to pull
                                                # in the dependency libs explicitly here since we're
                                                # not using libtool for the configure test.
                                                if test -f $with_sunldap/lib/libldap.la; then
                                                        LDAP_LIBS="`. $with_sunldap/lib/libldap.la; echo $dependency_libs` $LDAP_LIBS"
                                                fi
                                        else
                                                LDAP_LIBS="-llber $LDAP_LIBS"
                                        fi], LDAP_LIBS="", $LDAP_LDFLAGS $LDAP_LIBS)
                        fi
                        LDAP_LIBS="$LDAP_LDFLAGS $LDAP_LIBS"
                ], LDAP_LIBS="", $LDAP_LDFLAGS $LDAP_LIBS)

                if test -z "$LDAP_LIBS"; then
                       AC_MSG_ERROR(could not find SunLDAP libraries)
		fi

                AC_SUBST(LDAP_CFLAGS)
                AC_SUBST(LDAP_LIBS)
	fi
        AM_CONDITIONAL(ENABLE_LDAP, test $with_sunldap != no)
])

# EVO_PTHREAD_CHECK
AC_DEFUN([EVO_PTHREAD_CHECK],[
	PTHREAD_LIB=""
	AC_CHECK_LIB(pthread, pthread_create, PTHREAD_LIB="-lpthread",
		[AC_CHECK_LIB(pthreads, pthread_create, PTHREAD_LIB="-lpthreads",
		    [AC_CHECK_LIB(c_r, pthread_create, PTHREAD_LIB="-lc_r",
			[AC_CHECK_LIB(pthread, __pthread_attr_init_system, PTHREAD_LIB="-lpthread",
				[AC_CHECK_FUNC(pthread_create)]
			)]
		    )]
		)]
	)
	AC_SUBST(PTHREAD_LIB)
	AC_PROVIDE([EVO_PTHREAD_CHECK])
])
dnl -*- mode: autoconf -*-

dnl Usage:
dnl   GTK_DOC_CHECK([minimum-gtk-doc-version])
AC_DEFUN([GTK_DOC_CHECK],
[
  AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
  AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
  dnl for overriding the documentation installation directory
  AC_ARG_WITH([html-dir],
    AC_HELP_STRING([--with-html-dir=PATH], [path to installed docs]),,
    [with_html_dir='${datadir}/gtk-doc/html'])
  HTML_DIR="$with_html_dir"
  AC_SUBST(HTML_DIR)

  dnl enable/disable documentation building
  AC_ARG_ENABLE([gtk-doc],
    AC_HELP_STRING([--enable-gtk-doc],
                   [use gtk-doc to build documentation [default=no]]),,
    enable_gtk_doc=no)

  have_gtk_doc=no
  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi
  if test "$PKG_CONFIG" != "no" && $PKG_CONFIG --exists gtk-doc; then
    have_gtk_doc=yes
  fi

  dnl do we want to do a version check?
ifelse([$1],[],,
  [gtk_doc_min_version=$1
  if test "$have_gtk_doc" = yes; then
    AC_MSG_CHECKING([gtk-doc version >= $gtk_doc_min_version])
    if $PKG_CONFIG --atleast-version $gtk_doc_min_version gtk-doc; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
      have_gtk_doc=no
    fi
  fi
])
  if test x$enable_gtk_doc = xyes; then
    if test "$have_gtk_doc" != yes; then
      enable_gtk_doc=no
    fi
  fi

  AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)
  AM_CONDITIONAL(GTK_DOC_USE_LIBTOOL, test -n "$LIBTOOL")
])

# PILOT_LINK_CHECK
# Adds --with-pisock and determines the verion of the pisock
#

AC_SUBST(PISOCK_CFLAGS)
AC_SUBST(PISOCK_LIBS)

AC_DEFUN([PILOT_LINK_HOOK],[
	AC_ARG_WITH([pisock],
		    AC_HELP_STRING([--with-pisock],
		    		   [Specify prefix for pisock files]),
	[
	if test x$withval = xyes; then
	    dnl Note that an empty true branch is not valid sh syntax.
	    ifelse([$1], [], :, [$1])
	else
	    PISOCK_CFLAGS="-I$withval/include"
	    incdir="$withval/include"
	    PISOCK_LIBS="-L$withval/lib -lpisock -lpisync"
	    AC_MSG_CHECKING("for existance of $withval/lib/libpisock.so")
	    if test -r $withval/lib/libpisock.so; then
		AC_MSG_RESULT(yes)
	    else
		AC_MSG_ERROR([Unable to find libpisock. Try  http://www.pilot-link.org.])
	    fi
	fi
	])

	if test x$PISOCK_CFLAGS = x; then
	    AC_CHECK_HEADER(pi-version.h, [incdir="/usr/include"], [
	    AC_CHECK_HEADER(libpisock/pi-version.h, [PISOCK_CFLAGS="-I/usr/include/libpisock"
	                                             piversion_include="libpisock/pi-version.h"
						     incdir="/usr/include/libpisock"
                                                    ], [
	    AC_CHECK_HEADER($prefix/include/pi-version.h, [PISOCK_CFLAGS="-I$prefix/include/libpisock"
	                                                   piversion_include="$prefix/include/pi-version.h"
						           if test x$PISOCK_LIBDIR = x; then
							      incdir="$prefix/include"
							      PISOCK_LIBS="-L$prefix/lib -lpisock -lpisync"
                                                           fi							  ],
	    AC_MSG_ERROR([Unable to find pi-version.h])) 
	    ])
	    ])
	fi
		
	if test "x$PISOCK_LIBS" = "x"; then
		AC_CHECK_LIB(pisock, pi_accept, [ PISOCK_LIBS="-lpisock -lpisync"], 
			[ AC_MSG_ERROR([Unable to find libpisock. Try http://www.pilot-link.org.]) ])
	fi
	
	AC_ARG_ENABLE([pilotlinktest],
		AC_HELP_STRING([--enable-pilotlinktest],
			       [Test for correct version of pilot-link]),
		[testplversion=$enableval],
		[testplversion=yes]
	)

	if test x$piversion_include = x; then
		piversion_include="pi-version.h"
	fi

	pi_major=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_VERSION'|sed 's/#define PILOT_LINK_VERSION \([[0-9]]*\)/\1/'`
	pi_minor=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_MAJOR'|sed 's/#define PILOT_LINK_MAJOR \([[0-9]]*\)/\1/'`
	pi_micro=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_MINOR'|sed 's/#define PILOT_LINK_MINOR \([[0-9]]*\)/\1/'`
	pi_patch=`cat $incdir/pi-version.h|grep '#define PILOT_LINK_PATCH'|sed 's/#define PILOT_LINK_PATCH \"\(.*\)\"/\1/'`

	PILOT_LINK_MAJOR="$pi_major"
	PILOT_LINK_MINOR="$pi_minor"
	PILOT_LINK_MICRO="$pi_micro"
	PILOT_LINK_PATCH="$pi_patch"
	PILOT_LINK_VERSION="$pi_major.$pi_minor.$pi_micro$pi_patch"

	if test x$testplversion = xyes; then
		AC_MSG_CHECKING([for pilot-link version >= $1])
		pl_ma=`echo $1|sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
		pl_mi=`echo $1|sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
		pl_mc=`echo $1|sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
		CFLAGS_save="$CFLAGS"
		CFLAGS="$CFLAGS $PISOCK_CFLAGS"
		AC_TRY_RUN(
			[
			#include <$piversion_include>
			int main(int argc,char *argv[]) {
				if (PILOT_LINK_VERSION == $pl_ma) {
					if (PILOT_LINK_MAJOR == $pl_mi) {
						if (PILOT_LINK_MINOR >= $pl_mc) {
							return 0;
				       	  	}
					} else if (PILOT_LINK_MAJOR > $pl_mi) {
						return 0;
					}
				} else if (PILOT_LINK_VERSION > $pl_ma) {
					return 0;
				}
				return 1;
			}
			],
			[AC_MSG_RESULT([yes (found $PILOT_LINK_VERSION)])],
			[AC_MSG_ERROR([pilot-link >= $1 required])],
			[AC_MSG_WARN([No action taken for crosscompile])]
		)
		CFLAGS="$CFLAGS_save"
	fi

	unset piversion_include
	unset pi_verion
	unset pi_major
	unset pi_minor
	unset pi_patch
	unset incdir
	unset pl_mi
	unset pl_ma
	unset pl_ve
])

AC_DEFUN([PILOT_LINK_CHECK],[
	PILOT_LINK_HOOK($1,[],nofailure)
])

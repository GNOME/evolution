dnl - Determine where and which version of mozilla is installed on the system
dnl - Author: Andrew Chatham

AC_DEFUN([AC_CHECK_MOZILLA],[
	has_mozilla=false

	CFLAGS=${CFLAGS--O}

	mozprefix="/usr"

	AC_ARG_WITH(mozilla,
	  [  --with-mozilla=dir      Specify the Mozilla directory],[
	  if test x$withval != x; then
	     mozprefix=$withval
	  fi
	], [
	  if test x$MOZILLA != x; then
	     mozprefix="$MOZILLA/dist"
	  fi
	])

	MOZILLA_INCLUDE_DIR="$mozprefix/include"
	MOZILLA_LIB_DIR="$mozprefix/lib"
	MOZILLA_BIN_DIR="$mozprefix/bin"
	MOZILLA_IDL_DIR="$mozprefix/idl"

	AC_MSG_CHECKING(for mozilla)

	if ! test -f $MOZILLA_INCLUDE_DIR/nsError.h; then
		AC_MSG_ERROR("Could not find header nsError.h in includes dir $MOZILLA_INCLUDE_DIR")
	fi

	if ! test -f $MOZILLA_LIB_DIR/libgtkembedmoz.so; then
		AC_MSG_ERROR("Could not find library libgtkembedmoz.so in lib dir $MOZILLA_LIB_DIR")
	fi

	if ! test -f $MOZILLA_IDL_DIR/nsISupports.idl; then
		AC_MSG_ERROR("Could not find nsISupports.idl in idl dir $MOZILLA_IDL_DIR")
	fi

	if test -f $MOZILLA_LIB_DIR/defaults/pref/all.js; then
		ALLJSLOC=$MOZILLA_LIB_DIR/defaults/pref/all.js
	else
		ALLJSLOC=$MOZILLA_FIVE_HOME/defaults/pref/all.js
	fi

	MOZILLA_VER=$(perl $srcdir/macros/mozilla-version.pl $ALLJSLOC)

	if test $MOZILLA_VER -eq "0"; then 
		AC_MSG_ERROR("Could not determine mozilla version")
	fi

	if test $MOZILLA_VER -lt "91"; then
		AC_MSG_ERROR("Mozilla version must be at least 0.9.1")
	else
		AC_MSG_RESULT(found)
	fi

	AC_SUBST(MOZILLA_BIN_DIR)
	AC_SUBST(MOZILLA_LIB_DIR)
	AC_SUBST(MOZILLA_INCLUDE_DIR)
	AC_SUBST(MOZILLA_IDL_DIR)
	AC_SUBST(MOZILLA_VER)
])



AC_DEFUN([GNOME_CHECK_OBJC],
[
dnl Look for an ObjC compiler.
dnl FIXME: extend list of possible names of ObjC compilers.
	AC_CHECK_PROGS(OBJC, $OBJC gcc, "")


	oLIBS=$LIBS
	LIBS="$LIBS $PTHREAD_LIB"
	AC_CHECK_FUNC(sched_yield,,[
	  AC_CHECK_LIB(posix4,sched_yield,PTHREAD_LIB="$PTHREAD_LIB -lposix4")])
	LIBS=$oLIBS

	AC_CACHE_CHECK([if Objective C compiler ($OBJC) works],
		       ac_cv_prog_objc_works, [
	if test -n "$OBJC"; then
	   cat > conftest.m <<EOF
	#include <objc/Object.h>
	@interface myRandomObj : Object
	{
	}
	@end
	@implementation myRandomObj
	@end
	int main () {
	  /* No, you are not seeing double.  Remember that square brackets
	     are the autoconf m4 quotes.  */
	  id myid = [[myRandomObj alloc]];
	  [[myid free]];
	  return 0;
	}
	EOF

	   $OBJC -o conftest $LDFLAGS conftest.m -lobjc $PTHREAD_LIB 1>&AC_FD_CC 2>&1
	   result=$?
	   rm -f conftest*

	   if test $result -eq 0; then
	      ac_cv_prog_objc_works=yes
	   fi
	else
	   ac_cv_prog_objc_works=no
	fi
	])

	AM_CONDITIONAL(OBJECTIVE_C, test x$ac_cv_prog_objc_works = xyes)
])
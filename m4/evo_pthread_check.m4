dnl EVO_PTHREAD_CHECK
#serial 0.1
AC_DEFUN([EVO_PTHREAD_CHECK],[
	PTHREAD_LIB=""
	AC_CHECK_LIB(pthread, pthread_create, [PTHREAD_LIB="-lpthread"],
		[AC_CHECK_LIB(pthreads, pthread_create, [PTHREAD_LIB="-lpthreads"],
			[AC_CHECK_LIB(c_r, pthread_create, [PTHREAD_LIB="-lc_r"],
			[AC_CHECK_LIB(pthread, __pthread_attr_init_system, [PTHREAD_LIB="-lpthread"],
				[AC_CHECK_FUNC(pthread_create)]
			)]
			)]
		)]
	)
	AC_SUBST(PTHREAD_LIB)
	AC_PROVIDE([EVO_PTHREAD_CHECK])
])

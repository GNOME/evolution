AC_DEFUN([GNOME_PTHREAD_CHECK],[
	PTHREAD_LIB=""
	AC_CHECK_LIB(pthread, pthread_create, PTHREAD_LIB="-lpthread",
		[AC_CHECK_LIB(c_r, pthread_create, PTHREAD_LIB="-lc_r")])
	AC_SUBST(PTHREAD_LIB)
	AC_PROVIDE([GNOME_PTHREAD_CHECK])
])

AC_DEFUN([GNOME_GHTTP_CHECK],[
	GHTTP_LIB=""
	AC_REQUIRE([GNOME_INIT_HOOK])
	AC_CHECK_LIB(ghttp, ghttp_request_new, GHTTP_LIB="-lghttp",
                     ,-L$gnome_prefix)
	AC_SUBST(GHTTP_LIB)
	AC_PROVIDE([GNOME_GHTTP_CHECK])
])

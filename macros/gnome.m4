AC_DEFUN([GNOME_CHECK_GNOME],
[
	AC_REQUIRE([GNOME_X_CHECKS])
	AC_CHECK_LIB(gnome, gnome_init, [GNOME_LIBS="-lgnome -lgnomeui"],[
		AC_MSG_ERROR(Gnome libraries not found)],[
		"$GTK_LIBS $x_libs"])	
])

AC_DEFUN([GNOME_CHECK_CXX],
[
	AC_CHECK_PROG(cxx_found, c++, yes, no)
	AC_CHECK_PROG(cxx_found, g++, yes, no)
	AC_CHECK_PROG(cxx_found, CC, yes, no)
	AC_CHECK_PROG(cxx_found, cxx, yes, no)
	AC_CHECK_PROG(cxx_found, cc++, yes, no)

	if test "x$cxx_found" = "xyes"; then
		AC_PROG_CXX
	else
		AC_MSG_WARN(No C++ compiler - gnometris will not be built!)
	fi
	AM_CONDITIONAL(CXX_PRESENT, test "x$cxx_found" = xyes)
])

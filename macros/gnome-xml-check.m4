dnl
dnl Check for availability of the libxml library
dnl the XML parser uses libz if available too
dnl
AC_DEFUN([GNOME_XML_CHECK],[
	dnl Checks for zlib library.
	Z_LIBS=
	AC_CHECK_LIB(z, inflate,
	  AC_CHECK_HEADER(zlib.h, Z_LIBS="-lz"))

	AC_REQUIRE([GNOME_INIT_HOOK])
	GNOME_XML_LIB=""
	AC_CHECK_LIB(xml, xmlNewDoc, GNOME_XML_LIB="-lxml",
	             ,-L$gnome_prefix $Z_LIBS)
	AC_SUBST(GNOME_XML_LIB)
	AC_PROVIDE([GNOME_XML_CHECK])
])

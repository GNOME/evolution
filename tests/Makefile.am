# process this file with automake to create Makefile.in

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir)/camel \
	-I$(includedir) -I$(top_srcdir)/camel/providers/MH
LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS)
#	$(BONOBO_LIBS)				




test4_LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/camel/providers/MH/libcamelmh.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS)


noinst_PROGRAMS =  	\
	test1		\
	test2		\
	test3		\
	test4		\
	test7

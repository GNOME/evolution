# process this file with automake to create Makefile.in

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/camel \
	-I$(includedir) -I$(top_srcdir)/camel/providers/MH \
	-I$(top_srcdir)/camel/providers/mbox
LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(PTHREAD_LIB) $(EXTRA_GNOME_LIBS_THREADS)
#	$(BONOBO_LIBS)				




#test4_LDADD = \
#	$(top_builddir)/camel/libcamel.la 	\
#	$(top_builddir)/camel/providers/MH/libcamelmh.la 	\
#	$(GNOME_LIBDIR) 			\
#	$(GNOMEUI_LIBS) $(INTLLIBS) $(PTHREAD_LIB)

test9_LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/camel/providers/mbox/libcamelmbox.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(PTHREAD_LIB) $(EXTRA_GNOME_LIBS_THREADS)


noinst_PROGRAMS =  	\
	test1		\
	test2		\
	test3		\
	test7		\
	test8		\
	test9		\
	test10

# process this file with automake to create Makefile.in

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/camel \
	-I$(includedir) -I$(top_srcdir)/camel/providers/MH \
	-I$(top_srcdir)/camel/providers/mbox
LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/libibex/libibex.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)
#	$(BONOBO_LIBS)				




#test4_LDADD = \
#	$(top_builddir)/camel/libcamel.la 	\
#	$(top_builddir)/camel/providers/MH/libcamelmh.la 	\
#	$(GNOME_LIBDIR) 			\
#	$(GNOMEUI_LIBS) $(INTLLIBS) $(PTHREAD_LIB)

#test11_LDADD = \
#	$(LDADD) \
#	$(top_builddir)/filter/libfilter.la \
#	$(top_builddir)/libibex/libibex.la

test9_LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/libibex/libibex.la 	\
	$(top_builddir)/camel/providers/mbox/libcamelmbox.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)

if ENABLE_THREADS
THREAD_RELATED_TESTS=test8
else
THREAD_RELATED_TESTS=
endif


noinst_PROGRAMS =  	\
	test1		\
	test2		\
	test3		\
	test7		\
	test9		\
	test10		\
	test11		\
	test-formatter	\
	$(THREAD_RELATED_TESTS)

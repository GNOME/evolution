# process this file with automake to create Makefile.in

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/camel \
	-I$(includedir) -I$(top_srcdir)/camel/providers/pop3 \
	-I$(top_srcdir)/camel/providers/mbox -I$(top_srcdir)/camel/providers/nntp \
	-DG_LOG_DOMAIN=\"evolution-tests\"	

LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/e-util/libeutil.la	\
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
#	$(top_builddir)/libibex/libibex.a

#test9_LDADD = \
#	$(top_builddir)/camel/libcamel.la 	\
#	$(top_builddir)/camel/providers/mbox/libcamelmbox.la 	\
#	$(top_builddir)/libibex/libibex.la 	\
#	$(GNOME_LIBDIR) 			\
#	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)

test14_LDADD = \
	$(top_builddir)/camel/providers/vee/libcamelvee.la 	\
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/libibex/libibex.la 	\
	$(GNOME_LIBDIR) 			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)

#test12_LDADD = \
#	$(top_builddir)/camel/libcamel.la 	\
#	$(top_builddir)/camel/providers/nntp/libcamelnntp.la 	\
#	$(top_builddir)/libibex/libibex.la 	\
#	$(GNOME_LIBDIR) 			\
#	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)

test_movemail_LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/libibex/libibex.la 	\
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
	test10		\
	test11		\
	test13		\
	test14		\
	test-movemail	\
	test-url	\
	$(THREAD_RELATED_TESTS)

#	test3		\
#	test12		\
#	test-formatter	

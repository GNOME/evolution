
INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/camel \
	-I$(includedir) \
	-I$(top_srcdir)/camel/tests/lib \
	-DG_LOG_DOMAIN=\"evolution-tests\"	

LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/libibex/libibex.la 	\
	$(DB3_LDADD)				\
	$(GNOME_LIBDIR) 			\
	$(top_builddir)/camel/tests/lib/libcameltest.a			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)

check_PROGRAMS =  	\
	test1	test4	test5	\
	test2	test6	test7	\
	test3			\
	test8			\
	test9

TESTS = test1 test4 test5 \
	test2 test6 test7 \
	test3		  \
	test8		  \
	test9





INCLUDES =						\
	-I$(includedir)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl			 	\
	-I$(top_srcdir)/e-util			 	\
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)/camel/tests/lib 		\
	-DG_LOG_DOMAIN=\"evolution-tests\"		\
	$(CAMEL_CFLAGS)

LDADD = \
	$(top_builddir)/camel/libcamel.la 		\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/camel/tests/lib/libcameltest.a	\
	$(INTLLIBS) 					\
	$(EVOLUTION_MAIL_LIBS)

check_PROGRAMS =  	\
	test1	test2	test3	\
	test4	test5	test6	\
	test7	test8	test9	\
	test10  test11

TESTS = test1 	test2 	test3 	\
	test4 	test5 	test6 	\
	test7	test8	test9	\
	test10  test11

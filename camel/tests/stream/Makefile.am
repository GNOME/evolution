
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
	$(GNOME_LIBDIR) 				\
	$(top_builddir)/camel/tests/lib/libcameltest.a	\
	$(GNOMEUI_LIBS) 				\
	$(INTLLIBS) 					\
	$(EVOLUTION_MAIL_LIBS)

check_PROGRAMS =  	\
	test1 test2 test3

TESTS = test1 test2 test3




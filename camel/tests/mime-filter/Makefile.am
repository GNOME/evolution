
INCLUDES =						\
	-I$(includedir)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl			 	\
	-I$(top_srcdir)/e-util			 	\
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)/camel/tests/lib 		\
	-DG_LOG_DOMAIN=\"evolution-tests\"		\
	-DSOURCEDIR=\"$(srcdir)\"				\
	$(CAMEL_CFLAGS)

LDADD = \
	$(top_builddir)/camel/libcamel.la 		\
	$(top_builddir)/e-util/libeutil.la		\
	$(GNOME_LIBDIR) 				\
	$(top_builddir)/camel/tests/lib/libcameltest.a	\
	$(GNOMEUI_LIBS) 				\
	$(INTLLIBS) 					\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST =			\
	crlf-1.in		\
	crlf-1.out

check_PROGRAMS =  		\
	test-crlf

TESTS = test-crlf





INCLUDES =						\
	-I$(includedir)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl			 	\
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)/camel/tests/lib 		\
	-DG_LOG_DOMAIN=\"evolution-tests\"		\
	$(CAMEL_CFLAGS)

LDADD = \
	$(top_builddir)/camel/libcamel-provider.la 	\
	$(top_builddir)/camel/libcamel.la 		\
	$(top_builddir)/libedataserver/libedataserver-${BASE_VERSION}.la \
	$(top_builddir)/camel/tests/lib/libcameltest.a	\
	$(INTLLIBS)

check_PROGRAMS =  	\
	url		\
	url-scan	\
	utf7		\
	split		\
	test2

TESTS = url utf7 split url-scan test2




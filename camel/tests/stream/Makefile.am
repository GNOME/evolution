
INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/camel \
	-I$(includedir) \
	-I$(top_srcdir)/camel/tests/lib \
	-DG_LOG_DOMAIN=\"evolution-tests\"	

LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/libibex/libibex.la 	\
	$(GNOME_LIBDIR) 			\
	$(top_builddir)/camel/tests/lib/libcameltest.a			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)

check_PROGRAMS =  	\
	test1 test2 test3

TESTS = test1 test2 test3




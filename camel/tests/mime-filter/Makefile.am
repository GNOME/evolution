
INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/camel \
	-I$(includedir) \
	-I$(top_srcdir)/camel/tests/lib \
	-DG_LOG_DOMAIN=\"evolution-tests\" \
	-DSOURCEDIR=\"$(srcdir)\"

LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(top_builddir)/e-util/libeutil.la	\
	$(top_builddir)/libibex/libibex.la 	\
	$(GNOME_LIBDIR) 			\
	$(top_builddir)/camel/tests/lib/libcameltest.a			\
	$(GNOMEUI_LIBS) $(INTLLIBS) $(MAILER_LIBS)

EXTRA_DIST =			\
	crlf-1.in		\
	crlf-1.out		\
	stripheader-1.in	\
	stripheader-1.out	\
	stripheader-2.in	\
	stripheader-2.out	\
	stripheader-3.in	\
	stripheader-3.out	\
	stripheader-4.in	\
	stripheader-4.out	\
	stripheader-5.in	\
	stripheader-5.out	\
	stripheader-6.in	\
	stripheader-6.out

check_PROGRAMS =  		\
	test-crlf		\
	test-stripheader

TESTS = test-crlf test-stripheader




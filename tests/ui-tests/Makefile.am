# process this file with automake to create Makefile.in

INCLUDES =					\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/camel			\
	-I$(includedir)				\
	-I$(top_srcdir)/camel/providers/MH	\
	-I$(top_srcdir)/widgets			\
	$(EXTRA_GNOME_CFLAGS)

LDADD =							\
	$(top_builddir)/widgets/libevolutionwidgets.la	\
	$(top_builddir)/camel/libcamel.la		\
	$(INTLLIBS)					\
	$(EXTRA_GNOME_LIBS)				\
	$(PTHREAD_LIB)

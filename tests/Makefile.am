# process this file with automake to create Makefile.in

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir)/camel \
	-I$(includedir) 

LDADD = \
	$(top_builddir)/camel/libcamel.la \
	$(GNOME_LIBDIR) 		\
	$(GNOMEUI_LIBS) $(INTLLIBS)

noinst_PROGRAMS =  \
	test1


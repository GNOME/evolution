# process this file with automake to create Makefile.in

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir)/camel \
	-I$(includedir) -I$(top_srcdir)/camel/providers/MH \
	$(EXTRA_GNOME_CFLAGS)

LDADD = \
	$(top_builddir)/camel/libcamel.la 	\
	$(INTLLIBS) $(EXTRA_GNOME_LIBS)





noinst_PROGRAMS =  	\
	store_listing
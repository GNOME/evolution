## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelnntpincludedir = $(includedir)/camel

providerdir = $(pkglibdir)/camel-providers/$(VERSION)

provider_LTLIBRARIES = libcamelnntp.la
provider_DATA = libcamelnntp.urls

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR) -I$(top_srcdir)/camel	\
	-I$(srcdir)/../mbox			\
	-DG_LOG_DOMAIN=\"camel-nntp-provider\"

libcamelnntp_la_SOURCES = 			\
	camel-nntp-folder.c			\
	camel-nntp-newsrc.c			\
	camel-nntp-provider.c			\
	camel-nntp-store.c			\
	camel-nntp-utils.c

libcamelnntpinclude_HEADERS =			\
	camel-nntp-folder.h			\
	camel-nntp-newsrc.h			\
	camel-nntp-store.h

libcamelnntp_la_LDFLAGS = -version-info 0:0:0

EXTRA_DIST = libcamelnntp.urls

#noinst_PROGRAMS = test-newsrc

#LDADD = \
	#$(top_builddir)/camel/libcamel.la 	\
	#$(top_builddir)/e-util/libeutil.la	\
	#$(top_builddir)/libibex/libibex.la 	\
	#$(GNOME_LIBDIR) 			\
	#$(GNOMEUI_LIBS) $(INTLLIBS) $(EXTRA_GNOME_LIBS)
#	$(BONOBO_LIBS)				

#test_newsrc_LDADD = libcamelnntp.la $(LDADD)
